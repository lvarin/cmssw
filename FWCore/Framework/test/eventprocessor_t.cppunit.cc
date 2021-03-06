/*----------------------------------------------------------------------

Test of the EventProcessor class.

----------------------------------------------------------------------*/

#include "DataFormats/Provenance/interface/ModuleDescription.h"
#include "FWCore/Framework/interface/EventProcessor.h"
#include "FWCore/Framework/test/stubs/TestBeginEndJobAnalyzer.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ProcessDesc.h"
#include "FWCore/ParameterSet/interface/Registry.h"
#include "FWCore/PluginManager/interface/PresenceFactory.h"
#include "FWCore/PluginManager/interface/ProblemTracker.h"
#include "FWCore/PythonParameterSet/interface/PythonProcessDesc.h"
#include "FWCore/RootAutoLibraryLoader/interface/RootAutoLibraryLoader.h"
//I need to open a 'back door' in order to test the functionality
#include "FWCore/ServiceRegistry/interface/ActivityRegistry.h"
#define private public
#include "FWCore/ServiceRegistry/interface/ServiceRegistry.h"
#undef private
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Utilities/interface/Presence.h"

#include "cppunit/extensions/HelperMacros.h"

#include "boost/regex.hpp"

#include <exception>
#include <iostream>
#include <sstream>
#include <string>

// defined in the other cppunit
void doInit();

class testeventprocessor: public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(testeventprocessor);
  CPPUNIT_TEST(parseTest);
  CPPUNIT_TEST(beginEndTest);
  CPPUNIT_TEST(cleanupJobTest);
  CPPUNIT_TEST(activityRegistryTest);
  CPPUNIT_TEST(moduleFailureTest);
  CPPUNIT_TEST(endpathTest);
  CPPUNIT_TEST(asyncTest);
  CPPUNIT_TEST(serviceConfigSaveTest);
  CPPUNIT_TEST_SUITE_END();

 public:

  void setUp() {
    //std::cout << "setting up testeventprocessor" << std::endl;
    doInit();
    m_handler = std::auto_ptr<edm::AssertHandler>(new edm::AssertHandler());
    sleep_secs_ = 0;
  }

  void tearDown() { m_handler.reset();}
  void parseTest();
  void beginEndTest();
  void cleanupJobTest();
  void activityRegistryTest();
  void moduleFailureTest();
  void endpathTest();
  void serviceConfigSaveTest();

  void asyncTest();
  bool asyncRunAsync(edm::EventProcessor& ep);
  bool asyncRunTimeout(edm::EventProcessor& ep);
  void driveAsyncTest(bool (testeventprocessor::*)(edm::EventProcessor&),
                       std::string const& config_string);

 private:
  std::auto_ptr<edm::AssertHandler> m_handler;
  void work() {
    //std::cout << "work in testeventprocessor" << std::endl;
    std::string configuration(
      "import FWCore.ParameterSet.Config as cms\n"
      "process = cms.Process('p')\n"
      "process.maxEvents = cms.untracked.PSet(\n"
      "    input = cms.untracked.int32(5))\n"
      "process.source = cms.Source('EmptySource')\n"
      "process.m1 = cms.EDProducer('TestMod',\n"
      "    ivalue = cms.int32(10))\n"
      "process.m2 = cms.EDProducer('TestMod',\n"
      "    ivalue = cms.int32(-3))\n"
      "process.p1 = cms.Path(process.m1*process.m2)\n");
    edm::EventProcessor proc(configuration, true);
    proc.beginJob();
    proc.run();
    proc.endJob();
  }
  int sleep_secs_;
};

///registration of the test so that the runner can find it
CPPUNIT_TEST_SUITE_REGISTRATION(testeventprocessor);

static std::string makeConfig(int event_count) {
  static std::string const start =
      "import FWCore.ParameterSet.Config as cms\n"
      "process = cms.Process('p')\n"
      "process.MessageLogger = cms.Service('MessageLogger',\n"
      "    destinations = cms.untracked.vstring(\n"
      "        'cout',\n"
      "        'cerr'),\n"
      "    categories = cms.untracked.vstring(\n"
      "        'FwkJob',\n"
      "        'FwkReport'),\n"
      "    cout = cms.untracked.PSet(\n"
      "        threshold = cms.untracked.string('INFO'),\n"
      "        FwkReport = cms.untracked.PSet(\n"
      "            limit = cms.untracked.int32(0))),\n"
      "    cerr = cms.untracked.PSet(\n"
      "        threshold = cms.untracked.string('INFO'),\n"
      "        FwkReport = cms.untracked.PSet(\n"
      "            limit = cms.untracked.int32(0)))\n"
      ")\n"
      "process.maxEvents = cms.untracked.PSet(\n"
      "    input = cms.untracked.int32(";
  static std::string const finish =
      "))\n"
      "process.source = cms.Source('EmptySource')\n"
      "process.m1 = cms.EDProducer('IntProducer',\n"
      "    ivalue = cms.int32(10))\n"
      "process.p1 = cms.Path(process.m1)\n";

  std::ostringstream ost;
  ost << start << event_count << finish;
  return ost.str();
}

void testeventprocessor::asyncTest() {
  std::string test_config_2 = makeConfig(2);
  std::string test_config_80k = makeConfig(20000);

  // Load the message service plug-in
  boost::shared_ptr<edm::Presence> theMessageServicePresence;
  try {
    theMessageServicePresence =
      boost::shared_ptr<edm::Presence>(edm::PresenceFactory::get()->makePresence("MessageServicePresence").release());
  } catch(std::exception& e) {
    std::cerr << e.what() << std::endl;
    return;
  }

  sleep_secs_ = 0;
  std::cerr << "asyncRunAsync 2 event\n";
  driveAsyncTest(&testeventprocessor::asyncRunAsync, test_config_2);
  std::cerr << "asyncRunAsync 80k event\n";
  driveAsyncTest(&testeventprocessor::asyncRunAsync, test_config_80k);
  sleep_secs_ = 3;
  std::cerr << "asyncRunAsync 2 event with sleep 3\n";
  driveAsyncTest(&testeventprocessor::asyncRunAsync, test_config_2);
  sleep_secs_ = 0;
  std::cerr << "asyncRunTimeout 80k event\n";
  // cannot run the following test from scram because of a runtime
  // library error:
  // libgcc_s.so.1 must be installed for pthread_cancel to work
  // driveAsyncTest(&testeventprocessor::asyncRunTimeout, test_config_80k);
}

bool testeventprocessor::asyncRunAsync(edm::EventProcessor& ep) {
  for(int i = 0; i < 7; ++i) {
      try {
        ep.setRunNumber(i + 1);
      } catch(cms::Exception& e) {
      }
      ep.runAsync();
      if(sleep_secs_ > 0) sleep(sleep_secs_);

      edm::EventProcessor::StatusCode rc = edm::EventProcessor::StatusCode();
      if (i < 2) {
        rc = ep.waitTillDoneAsync(1000);
      }
      else if (i == 2) {
        rc = ep.stopAsync(1000);
      }
      else if (i == 3) {
        rc = ep.stopAsync(1000);
        rc = ep.waitTillDoneAsync(1000);
      }
      else if (i == 4) {
        rc = ep.waitTillDoneAsync(1000);
        rc = ep.stopAsync(1000);
      }
      else if (i == 5) {
        rc = ep.waitTillDoneAsync(1000);
        rc = ep.waitTillDoneAsync(1000);
      }
      else if (i == 6) {
        rc = ep.stopAsync(1000);
        rc = ep.stopAsync(1000);
      }
      std::cerr << " ep runAsync run " << i << " done\n";

      switch(rc) {
        case edm::EventProcessor::epSuccess:
        case edm::EventProcessor::epInputComplete:
          break;
        case edm::EventProcessor::epTimedOut:
        default:
        {
            std::cerr << "rc from run " << i << ", doneAsync = " << rc << "\n";
            CPPUNIT_ASSERT("Bad rc from doneAsync" == 0);
        }
      }
    }
  return true;
}

bool testeventprocessor::asyncRunTimeout(edm::EventProcessor& ep) {
  try {
  ep.setRunNumber(1);
  } catch(cms::Exception& e) {
  }
  ep.runAsync();
  edm::EventProcessor::StatusCode rc = ep.waitTillDoneAsync(1);
  std::cerr << " ep runAsync run " << 1 << " done\n";

  switch(rc) {
    case edm::EventProcessor::epTimedOut:
      break;
    case edm::EventProcessor::epSuccess:
    case edm::EventProcessor::epInputComplete:
      break;
    default:
    {
      std::cerr << "rc from run " << 1 << ", doneAsync = " << rc << "\n";
      CPPUNIT_ASSERT("Bad rc from doneAsync" == 0);
    }
  }
  return false;
}

void testeventprocessor::driveAsyncTest(bool(testeventprocessor::*func)(edm::EventProcessor& ep), std::string const& config_str) {

  try {
    edm::EventProcessor proc(config_str, true);
    proc.beginJob();
    if ((this->*func)(proc)) {
      proc.endJob();
    } else {
        std::cerr << "event processor is in error state\n";
      }
  }
  catch(cms::Exception& e) {
      std::cerr << "cms exception: " << e.explainSelf() << std::endl;
      CPPUNIT_ASSERT("cms exeption" == 0);
  }
  catch(std::exception& e) {
      std::cerr << "std exception: " << e.what() << std::endl;
      CPPUNIT_ASSERT("std exeption" == 0);
  }
  catch(...) {
      std::cerr << "unknown exception " << std::endl;
      CPPUNIT_ASSERT("unknown exeption" == 0);
  }
  std::cerr << "*********************** driveAsyncTest ending ------\n";
}

void testeventprocessor::parseTest() {
  try { work();}
  catch (cms::Exception& e) {
      std::cerr << "cms exception caught: "
                << e.explainSelf() << std::endl;
      CPPUNIT_ASSERT("Caught cms::Exception " == 0);
  }
  catch (std::exception& e) {
      std::cerr << "Standard library exception caught: "
                << e.what() << std::endl;
      CPPUNIT_ASSERT("Caught std::exception " == 0);
  }
  catch (...) {
      CPPUNIT_ASSERT("Caught unknown exception " == 0);
  }
}

void testeventprocessor::beginEndTest() {
  std::string configuration(
      "import FWCore.ParameterSet.Config as cms\n"
      "process = cms.Process('p')\n"
      "process.maxEvents = cms.untracked.PSet(\n"
      "    input = cms.untracked.int32(10))\n"
      "process.source = cms.Source('EmptySource')\n"
      "process.m1 = cms.EDAnalyzer('TestBeginEndJobAnalyzer')\n"
      "process.p1 = cms.Path(process.m1)\n");
  {
    //std::cout << "beginEndTest 1" << std::endl;
    TestBeginEndJobAnalyzer::control().beginJobCalled = false;
    TestBeginEndJobAnalyzer::control().endJobCalled = false;
    TestBeginEndJobAnalyzer::control().beginRunCalled = false;
    TestBeginEndJobAnalyzer::control().endRunCalled = false;
    TestBeginEndJobAnalyzer::control().beginLumiCalled = false;
    TestBeginEndJobAnalyzer::control().endLumiCalled = false;

    edm::EventProcessor proc(configuration, true);

    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginJobCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endJobCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginRunCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endRunCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginLumiCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endLumiCalled);
    CPPUNIT_ASSERT(0 == proc.totalEvents());

    proc.beginJob();
 
    //std::cout << "beginEndTest 1 af" << std::endl;
 
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginJobCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endJobCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginRunCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endRunCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginLumiCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endLumiCalled);
    CPPUNIT_ASSERT(0 == proc.totalEvents());

    proc.endJob();

    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginJobCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endJobCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginRunCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endRunCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginLumiCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endLumiCalled);
    CPPUNIT_ASSERT(0 == proc.totalEvents());

     CPPUNIT_ASSERT(not edm::pset::Registry::instance()->empty());
  }
  CPPUNIT_ASSERT(edm::pset::Registry::instance()->empty());

  {
    //std::cout << "beginEndTest 2" << std::endl;

    TestBeginEndJobAnalyzer::control().beginJobCalled = false;
    TestBeginEndJobAnalyzer::control().endJobCalled = false;
    TestBeginEndJobAnalyzer::control().beginRunCalled = false;
    TestBeginEndJobAnalyzer::control().endRunCalled = false;
    TestBeginEndJobAnalyzer::control().beginLumiCalled = false;
    TestBeginEndJobAnalyzer::control().endLumiCalled = false;

    edm::EventProcessor proc(configuration, true);
    proc.runToCompletion(false);

    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginJobCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endJobCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginRunCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endRunCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginLumiCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endLumiCalled);
    CPPUNIT_ASSERT(10 == proc.totalEvents());
    CPPUNIT_ASSERT(not edm::pset::Registry::instance()->empty());
  }
  CPPUNIT_ASSERT(edm::pset::Registry::instance()->empty());
  {
    //std::cout << "beginEndTest 3" << std::endl;

    TestBeginEndJobAnalyzer::control().beginJobCalled = false;
    TestBeginEndJobAnalyzer::control().endJobCalled = false;
    TestBeginEndJobAnalyzer::control().beginRunCalled = false;
    TestBeginEndJobAnalyzer::control().endRunCalled = false;
    TestBeginEndJobAnalyzer::control().beginLumiCalled = false;
    TestBeginEndJobAnalyzer::control().endLumiCalled = false;

    edm::EventProcessor proc(configuration, true);
    proc.beginJob();
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginJobCalled);

    // Check that beginJob is not called again
    TestBeginEndJobAnalyzer::control().beginJobCalled = false;

    proc.runToCompletion(false);

    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginJobCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endJobCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginRunCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endRunCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginLumiCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endLumiCalled);
    CPPUNIT_ASSERT(10 == proc.totalEvents());

    proc.endJob();

    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginJobCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endJobCalled);
    CPPUNIT_ASSERT(10 == proc.totalEvents());
  }
  {
    TestBeginEndJobAnalyzer::control().beginJobCalled = false;
    TestBeginEndJobAnalyzer::control().endJobCalled = false;
    TestBeginEndJobAnalyzer::control().beginRunCalled = false;
    TestBeginEndJobAnalyzer::control().endRunCalled = false;
    TestBeginEndJobAnalyzer::control().beginLumiCalled = false;
    TestBeginEndJobAnalyzer::control().endLumiCalled = false;

    edm::EventProcessor proc(configuration, true);
    proc.beginJob();

    // Check that beginJob is not called again
    TestBeginEndJobAnalyzer::control().beginJobCalled = false;

    proc.run();

    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginJobCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endJobCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginRunCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endRunCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginLumiCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endLumiCalled);
    CPPUNIT_ASSERT(10 == proc.totalEvents());
    
    proc.endJob();
    
    // Check that these are not called again
    TestBeginEndJobAnalyzer::control().endRunCalled = false;
    TestBeginEndJobAnalyzer::control().endLumiCalled = false;

  }
  CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endRunCalled);
  CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endLumiCalled);
  {
    TestBeginEndJobAnalyzer::control().beginJobCalled = false;
    TestBeginEndJobAnalyzer::control().endJobCalled = false;
    TestBeginEndJobAnalyzer::control().beginRunCalled = false;
    TestBeginEndJobAnalyzer::control().endRunCalled = false;
    TestBeginEndJobAnalyzer::control().beginLumiCalled = false;
    TestBeginEndJobAnalyzer::control().endLumiCalled = false;

    edm::EventProcessor proc(configuration, true);
    proc.run();

    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginJobCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endJobCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginRunCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endRunCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginLumiCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endLumiCalled);
    CPPUNIT_ASSERT(10 == proc.totalEvents());

    // Check that these are not called again
    TestBeginEndJobAnalyzer::control().beginJobCalled = false;
    TestBeginEndJobAnalyzer::control().beginRunCalled = false;
    TestBeginEndJobAnalyzer::control().beginLumiCalled = false;
    TestBeginEndJobAnalyzer::control().endRunCalled = false;
    TestBeginEndJobAnalyzer::control().endLumiCalled = false;

    proc.endJob();

    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginJobCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endJobCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginRunCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endRunCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().beginLumiCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endLumiCalled);
    CPPUNIT_ASSERT(10 == proc.totalEvents());

    // Check that these are not called again
    TestBeginEndJobAnalyzer::control().endRunCalled = false;
    TestBeginEndJobAnalyzer::control().endLumiCalled = false;
  }
  CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endRunCalled);
  CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endLumiCalled);

  {
    TestBeginEndJobAnalyzer::control().beginJobCalled = false;
    TestBeginEndJobAnalyzer::control().endJobCalled = false;
    TestBeginEndJobAnalyzer::control().beginRunCalled = false;
    TestBeginEndJobAnalyzer::control().endRunCalled = false;
    TestBeginEndJobAnalyzer::control().beginLumiCalled = false;
    TestBeginEndJobAnalyzer::control().endLumiCalled = false;

    edm::EventProcessor proc(configuration, true);
    proc.run();

    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginJobCalled);
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endJobCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginRunCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endRunCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().beginLumiCalled);
    CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endLumiCalled);
    CPPUNIT_ASSERT(10 == proc.totalEvents());
  }
  CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().endJobCalled);
  CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endRunCalled);
  CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().endLumiCalled);
}

void testeventprocessor::cleanupJobTest()
{
  //std::cout << "cleanup " << std::endl; 
  std::string configuration(
      "import FWCore.ParameterSet.Config as cms\n"
      "process = cms.Process('p')\n"
      "process.maxEvents = cms.untracked.PSet(\n"
      "    input = cms.untracked.int32(2))\n"
      "process.source = cms.Source('EmptySource')\n"
      "process.m1 = cms.EDAnalyzer('TestBeginEndJobAnalyzer')\n"
      "process.p1 = cms.Path(process.m1)\n");
  {
      //std::cout << "cleanup 1" << std::endl;

    TestBeginEndJobAnalyzer::control().destructorCalled = false;
    edm::EventProcessor proc(configuration, true);

    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().destructorCalled);
    proc.beginJob();
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().destructorCalled);
    proc.endJob();
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().destructorCalled);
  }
  CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().destructorCalled);
  {
      //std::cout << "cleanup 2" << std::endl;

    TestBeginEndJobAnalyzer::control().destructorCalled = false;
    edm::EventProcessor proc(configuration, true);

    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().destructorCalled);
    proc.run();
    CPPUNIT_ASSERT(2 == proc.totalEvents());
    CPPUNIT_ASSERT(!TestBeginEndJobAnalyzer::control().destructorCalled);

  }
  CPPUNIT_ASSERT(TestBeginEndJobAnalyzer::control().destructorCalled);
}

namespace {
  struct Listener{
    Listener(edm::ActivityRegistry& iAR) :
      postBeginJob_(0),
      postEndJob_(0),
      preEventProcessing_(0),
      postEventProcessing_(0),
      preModule_(0),
      postModule_(0) {
        iAR.watchPostBeginJob(this, &Listener::postBeginJob);
        iAR.watchPostEndJob(this, &Listener::postEndJob);

        iAR.watchPreProcessEvent(this, &Listener::preEventProcessing);
        iAR.watchPostProcessEvent(this, &Listener::postEventProcessing);

        iAR.watchPreModule(this, &Listener::preModule);
        iAR.watchPostModule(this, &Listener::postModule);
      }

    void postBeginJob() {++postBeginJob_;}
    void postEndJob() {++postEndJob_;}

    void preEventProcessing(edm::EventID const&, edm::Timestamp const&) {
      ++preEventProcessing_;}
    void postEventProcessing(edm::Event const&, edm::EventSetup const&) {
      ++postEventProcessing_;}

    void preModule(edm::ModuleDescription const&) {
      ++preModule_;
    }
    void postModule(edm::ModuleDescription const&) {
      ++postModule_;
    }

    unsigned int postBeginJob_;
    unsigned int postEndJob_;
    unsigned int preEventProcessing_;
    unsigned int postEventProcessing_;
    unsigned int preModule_;
    unsigned int postModule_;
  };
}

void
testeventprocessor::activityRegistryTest() {
  std::string configuration(
      "import FWCore.ParameterSet.Config as cms\n"
      "process = cms.Process('p')\n"
      "process.maxEvents = cms.untracked.PSet(\n"
      "    input = cms.untracked.int32(5))\n"
      "process.source = cms.Source('EmptySource')\n"
      "process.m1 = cms.EDProducer('TestMod',\n"
      "   ivalue = cms.int32(-3))\n"
      "process.p1 = cms.Path(process.m1)\n");

  boost::shared_ptr<edm::ParameterSet> parameterSet = PythonProcessDesc(configuration).parameterSet();
  boost::shared_ptr<edm::ProcessDesc> processDesc(new edm::ProcessDesc(parameterSet));

  //We don't want any services, we just want an ActivityRegistry to be created
  // We then use this ActivityRegistry to 'spy on' the signals being produced
  // inside the EventProcessor
  std::vector<edm::ParameterSet> serviceConfigs;
  edm::ServiceToken token = edm::ServiceRegistry::createSet(serviceConfigs);

  edm::ActivityRegistry ar;
  token.connect(ar);
  Listener listener(ar);

  edm::EventProcessor proc(processDesc, token, edm::serviceregistry::kOverlapIsError);

  proc.beginJob();
  proc.run();
  proc.endJob();

  CPPUNIT_ASSERT(listener.postBeginJob_ == 1);
  CPPUNIT_ASSERT(listener.postEndJob_ == 1);
  CPPUNIT_ASSERT(listener.preEventProcessing_ == 5);
  CPPUNIT_ASSERT(listener.postEventProcessing_ == 5);
  CPPUNIT_ASSERT(listener.preModule_ == 10);
  CPPUNIT_ASSERT(listener.postModule_ == 10);

  typedef std::vector<edm::ModuleDescription const*> ModuleDescs;
  ModuleDescs allModules = proc.getAllModuleDescriptions();
  CPPUNIT_ASSERT(2 == allModules.size()); // TestMod and TriggerResultsInserter
  std::cout << "\nModuleDescriptions in testeventprocessor::activityRegistryTest()---\n";
  for (ModuleDescs::const_iterator i = allModules.begin(), e = allModules.end();
       i != e ;
       ++i) {
    CPPUNIT_ASSERT(*i != 0);
    std::cout << **i << '\n';
  }
  std::cout << "--- end of ModuleDescriptions\n";

  CPPUNIT_ASSERT(5 == proc.totalEvents());
  CPPUNIT_ASSERT(5 == proc.totalEventsPassed());
}

static
bool
findModuleName(std::string const& iMessage) {
  static boost::regex const expr("TestFailuresAnalyzer");
  return regex_search(iMessage, expr);
}

void
testeventprocessor::moduleFailureTest() {
  try {

    std::string const preC(
      "import FWCore.ParameterSet.Config as cms\n"
      "process = cms.Process('p')\n"
      "process.maxEvents = cms.untracked.PSet(\n"
      "    input = cms.untracked.int32(2))\n"
      "process.source = cms.Source('EmptySource')\n"
      "process.m1 = cms.EDAnalyzer('TestFailuresAnalyzer',\n"
      "    whichFailure = cms.int32(");

    std::string const postC(
      "))\n"
      "process.p1 = cms.Path(process.m1)\n");

    {
      std::string const configuration = preC +"0"+postC;
      bool threw = true;
      try {
        edm::EventProcessor proc(configuration, true);
        threw = false;
      } catch(cms::Exception const& iException) {
        if(!findModuleName(iException.explainSelf())) {
          std::cout << iException.explainSelf() << std::endl;
          CPPUNIT_ASSERT(0 == "module name not in exception message");
        }
      }
      CPPUNIT_ASSERT(threw && 0 != "exception never thrown");
    }
    {
      std::string const configuration = preC +"1"+postC;
      bool threw = true;
      edm::EventProcessor proc(configuration, true);

      try {
        proc.beginJob();
        threw = false;
      } catch(cms::Exception const& iException) {
        if(!findModuleName(iException.explainSelf())) {
          std::cout << iException.explainSelf() << std::endl;
          CPPUNIT_ASSERT(0 == "module name not in exception message");
        }
      }
      CPPUNIT_ASSERT(threw && 0 != "exception never thrown");
    }

    {
      std::string const configuration = preC +"2"+postC;
      bool threw = true;
      edm::EventProcessor proc(configuration, true);

      proc.beginJob();
      try {
        proc.run();
        threw = false;
      } catch(cms::Exception const& iException) {
        if(!findModuleName(iException.explainSelf())) {
          std::cout << iException.explainSelf() << std::endl;
          CPPUNIT_ASSERT(0 == "module name not in exception message");
        }
      }
      CPPUNIT_ASSERT(threw && 0 != "exception never thrown");
      proc.endJob();
    }
    {
      std::string const configuration = preC +"3"+postC;
      bool threw = true;
      edm::EventProcessor proc(configuration, true);

      proc.beginJob();
      try {
        proc.endJob();
        threw = false;
      } catch(cms::Exception const& iException) {
        if(!findModuleName(iException.explainSelf())) {
          std::cout << iException.explainSelf() << std::endl;
          CPPUNIT_ASSERT(0 == "module name not in exception message");
        }
      }
      CPPUNIT_ASSERT(threw && 0 != "exception never thrown");
    }
    ///
    {
      bool threw = true;
      try {
        std::string configuration(
          "import FWCore.ParameterSet.Config as cms\n"
          "process = cms.Process('p')\n"
          "process.maxEvents = cms.untracked.PSet(\n"
          "    input = cms.untracked.int32(2))\n"
          "process.source = cms.Source('EmptySource')\n"
          "process.p1 = cms.Path(process.m1)\n");
        edm::EventProcessor proc(configuration, true);

        threw = false;
      } catch(cms::Exception const& iException) {
        static boost::regex const expr("m1");
        if(!regex_search(iException.explainSelf(), expr)) {
          std::cout << iException.explainSelf() << std::endl;
          CPPUNIT_ASSERT(0 == "module name not in exception message");
        }
      }
      CPPUNIT_ASSERT(threw && 0 != "exception never thrown");
    }
  } catch(cms::Exception const& iException) {
    std::cout << "Unexpected exception " << iException.explainSelf() << std::endl;
    throw;
  }
}

void
testeventprocessor::serviceConfigSaveTest() {
   std::string configuration(
                             "import FWCore.ParameterSet.Config as cms\n"
                             "process = cms.Process('p')\n"
                             "process.add_(cms.Service('DummyStoreConfigService'))\n"
                             "process.maxEvents = cms.untracked.PSet(\n"
                             "    input = cms.untracked.int32(5))\n"
                             "process.source = cms.Source('EmptySource')\n"
                             "process.m1 = cms.EDProducer('TestMod',\n"
                             "   ivalue = cms.int32(-3))\n"
                             "process.p1 = cms.Path(process.m1)\n");

   edm::EventProcessor proc(configuration, true);
   edm::ParameterSet topPset(edm::getProcessParameterSet());
   CPPUNIT_ASSERT(topPset.existsAs<edm::ParameterSet>("DummyStoreConfigService", true));
}

void
testeventprocessor::endpathTest() {
}
