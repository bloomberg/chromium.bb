// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/test/component_updater_service_unittest.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/component_updater/test/test_installer.h"
#include "chrome/common/chrome_paths.h"
#include "content/test/net/url_request_prepackaged_interceptor.h"
#include "libxml/globals.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

using content::BrowserThread;

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;

MockComponentObserver::MockComponentObserver() {
}

MockComponentObserver::~MockComponentObserver() {
}

TestConfigurator::TestConfigurator()
    : times_(1),
      recheck_time_(0),
      ondemand_time_(0),
      cus_(NULL),
      context_(new net::TestURLRequestContextGetter(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO))) {
}

TestConfigurator::~TestConfigurator() {
}

int TestConfigurator::InitialDelay() { return 0; }

int TestConfigurator::NextCheckDelay() {
  // This is called when a new full cycle of checking for updates is going
  // to happen. In test we normally only test one cycle so it is a good
  // time to break from the test messageloop Run() method so the test can
  // finish.
  if (--times_ <= 0) {
    base::MessageLoop::current()->Quit();
    return 0;
  }

  // Look for checks to issue in the middle of the loop.
  for (std::list<CheckAtLoopCount>::iterator
           i = components_to_check_.begin();
       i != components_to_check_.end(); ) {
    if (i->second == times_) {
      cus_->CheckForUpdateSoon(*i->first);
      i = components_to_check_.erase(i);
    } else {
      ++i;
    }
  }
  return 1;
}

int TestConfigurator::StepDelay() {
  return 0;
}

int TestConfigurator::StepDelayMedium() {
  return NextCheckDelay();
}

int TestConfigurator::MinimumReCheckWait() {
  return recheck_time_;
}

int TestConfigurator::OnDemandDelay() {
  return ondemand_time_;
}

GURL TestConfigurator::UpdateUrl() {
  return GURL("http://localhost/upd");
}

GURL TestConfigurator::PingUrl() {
  return GURL("http://localhost2/ping");
}

const char* TestConfigurator::ExtraRequestParams() { return "extra=foo"; }

size_t TestConfigurator::UrlSizeLimit() { return 256; }

net::URLRequestContextGetter* TestConfigurator::RequestContext() {
  return context_.get();
}

// Don't use the utility process to decode files.
bool TestConfigurator::InProcess() { return true; }

ComponentPatcher* TestConfigurator::CreateComponentPatcher() {
  return new MockComponentPatcher();
}

bool TestConfigurator::DeltasEnabled() const {
  return true;
}

// Set how many update checks are called, the default value is just once.
void TestConfigurator::SetLoopCount(int times) { times_ = times; }

void TestConfigurator::SetRecheckTime(int seconds) {
  recheck_time_ = seconds;
}

void TestConfigurator::SetOnDemandTime(int seconds) {
  ondemand_time_ = seconds;
}

void TestConfigurator::AddComponentToCheck(CrxComponent* com,
                                           int at_loop_iter) {
  components_to_check_.push_back(std::make_pair(com, at_loop_iter));
}

void TestConfigurator::SetComponentUpdateService(ComponentUpdateService* cus) {
  cus_ = cus;
}

ComponentUpdaterTest::ComponentUpdaterTest()
    : test_config_(NULL),
      ui_thread_(BrowserThread::UI, &message_loop_),
      file_thread_(BrowserThread::FILE),
      io_thread_(BrowserThread::IO) {
  // The component updater instance under test.
  test_config_ = new TestConfigurator;
  component_updater_.reset(ComponentUpdateServiceFactory(test_config_));
  test_config_->SetComponentUpdateService(component_updater_.get());

  // The test directory is chrome/test/data/components.
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("components");

  net::URLFetcher::SetEnableInterceptionForTests(true);

  io_thread_.StartIOThread();
  file_thread_.Start();
}

ComponentUpdaterTest::~ComponentUpdaterTest() {
  net::URLFetcher::SetEnableInterceptionForTests(false);
}

void ComponentUpdaterTest::TearDown() {
  xmlCleanupGlobals();
}

ComponentUpdateService* ComponentUpdaterTest::component_updater() {
  return component_updater_.get();
}

  // Makes the full path to a component updater test file.
const base::FilePath ComponentUpdaterTest::test_file(const char* file) {
  return test_data_dir_.AppendASCII(file);
}

TestConfigurator* ComponentUpdaterTest::test_configurator() {
  return test_config_;
}

ComponentUpdateService::Status ComponentUpdaterTest::RegisterComponent(
    CrxComponent* com,
    TestComponents component,
    const Version& version,
    TestInstaller* installer) {
  if (component == kTestComponent_abag) {
    com->name = "test_abag";
    com->pk_hash.assign(abag_hash, abag_hash + arraysize(abag_hash));
  } else if (component == kTestComponent_jebg) {
    com->name = "test_jebg";
    com->pk_hash.assign(jebg_hash, jebg_hash + arraysize(jebg_hash));
  } else {
    com->name = "test_ihfo";
    com->pk_hash.assign(ihfo_hash, ihfo_hash + arraysize(ihfo_hash));
  }
  com->version = version;
  com->installer = installer;
  return component_updater_->RegisterComponent(*com);
}

PingChecker::PingChecker(const std::map<std::string, std::string>& attributes)
    : num_hits_(0), num_misses_(0), attributes_(attributes) {
}

PingChecker::~PingChecker() {}

void PingChecker::Trial(net::URLRequest* request) {
  if (Test(request))
    ++num_hits_;
  else
    ++num_misses_;
}

bool PingChecker::Test(net::URLRequest* request) {
  if (request->has_upload()) {
    const net::UploadDataStream* stream = request->get_upload();
    const net::UploadBytesElementReader* reader =
        stream->element_readers()[0]->AsBytesReader();
    int size = reader->length();
    scoped_refptr <net::IOBuffer> buffer = new net::IOBuffer(size);
    std::string data(reader->bytes());
    pings_.push_back(data);
    // For now, we assume that there is only one ping per POST.
    std::string::size_type start = data.find("<o:event");
    if (start != std::string::npos) {
      std::string::size_type end = data.find(">", start);
      if (end != std::string::npos) {
        std::string ping = data.substr(start, end - start);
        std::map<std::string, std::string>::const_iterator iter;
        for (iter = attributes_.begin(); iter != attributes_.end(); ++iter) {
          // does the ping contain the specified attribute/value?
          if (ping.find(std::string(" ") + (iter->first) +
              std::string("=") + (iter->second)) == string::npos) {
            return false;
          }
        }
        return true;
      }
    }
  }
  return false;
}

std::string PingChecker::GetPings() const {
  std::string pings_str = "Pings are:";
  int i = 0;
  for (std::vector<std::string>::const_iterator it = pings_.begin();
      it != pings_.end(); ++it) {
    pings_str.append(base::StringPrintf("\n  (%d): %s", ++i, it->c_str()));
  }
  return pings_str;
}

// Verify that our test fixture work and the component updater can
// be created and destroyed with no side effects.
TEST_F(ComponentUpdaterTest, VerifyFixture) {
  EXPECT_TRUE(component_updater() != NULL);
}

// Verify that the component updater can be caught in a quick
// start-shutdown situation. Failure of this test will be a crash.
TEST_F(ComponentUpdaterTest, StartStop) {
  component_updater()->Start();
  message_loop_.RunUntilIdle();
  component_updater()->Stop();
}

// Verify that when the server has no updates, we go back to sleep and
// the COMPONENT_UPDATER_STARTED and COMPONENT_UPDATER_SLEEPING notifications
// are generated.
TEST_F(ComponentUpdaterTest, CheckCrxSleep) {
  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  MockComponentObserver observer;

  TestInstaller installer;
  CrxComponent com;
  com.observer = &observer;
  EXPECT_EQ(ComponentUpdateService::kOk,
            RegisterComponent(&com,
                              kTestComponent_abag,
                              Version("1.1"),
                              &installer));

  const GURL expected_update_url(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dabagagagagagagagagagagagagagagag%26v%3D1.1%26fp%3D%26uc");

  interceptor.SetResponse(expected_update_url,
                          test_file("updatecheck_reply_1.xml"));

  // We loop twice, but there are no updates so we expect two sleep messages.
  test_configurator()->SetLoopCount(2);

  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
              .Times(1);
  component_updater()->Start();

  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
              .Times(2);
  message_loop_.Run();

  EXPECT_EQ(2, interceptor.GetHitCount());

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  component_updater()->Stop();

  // Loop twice again but this case we simulate a server error by returning
  // an empty file.

  interceptor.SetResponse(expected_update_url,
                          test_file("updatecheck_reply_empty"));

  test_configurator()->SetLoopCount(2);

  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
              .Times(1);
  component_updater()->Start();

  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
              .Times(2);
  message_loop_.Run();

  EXPECT_EQ(4, interceptor.GetHitCount());

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  component_updater()->Stop();
}

// Verify that we can check for updates and install one component. Besides
// the notifications above COMPONENT_UPDATE_FOUND and COMPONENT_UPDATE_READY
// should have been fired. We do two loops so the second time around there
// should be nothing left to do.
// We also check that only 3 non-ping network requests are issued:
// 1- manifest check
// 2- download crx
// 3- second manifest check.
TEST_F(ComponentUpdaterTest, InstallCrx) {
  std::map<std::string, std::string> map;
  map.insert(std::pair<std::string, std::string>("eventtype", "\"3\""));
  map.insert(std::pair<std::string, std::string>("eventresult", "\"1\""));
  map.insert(std::pair<std::string, std::string>("previousversion",
                                                 "\"0.9\""));
  map.insert(std::pair<std::string, std::string>("nextversion", "\"1.0\""));
  PingChecker ping_checker(map);
  URLRequestPostInterceptor post_interceptor(&ping_checker);
  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  MockComponentObserver observer1;
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_FOUND, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_READY, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(2);
  }

  MockComponentObserver observer2;
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(2);
  }

  TestInstaller installer1;
  CrxComponent com1;
  com1.observer = &observer1;
  RegisterComponent(&com1, kTestComponent_jebg, Version("0.9"), &installer1);
  TestInstaller installer2;
  CrxComponent com2;
  com2.observer = &observer2;
  RegisterComponent(&com2, kTestComponent_abag, Version("2.2"), &installer2);

  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo"
      "&x=id%3Djebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26fp%3D%26uc"
      "&x=id%3Dabagagagagagagagagagagagagagagag%26v%3D2.2%26fp%3D%26uc");

  const GURL expected_update_url_2(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dabagagagagagagagagagagagagagagag%26v%3D2.2%26fp%3D%26uc"
      "&x=id%3Djebgalgnebhfojomionfpkfelancnnkf%26v%3D1.0%26fp%3D%26uc");

  interceptor.SetResponse(expected_update_url_1,
                          test_file("updatecheck_reply_1.xml"));
  interceptor.SetResponse(expected_update_url_2,
                          test_file("updatecheck_reply_1.xml"));
  interceptor.SetResponse(GURL(expected_crx_url),
                          test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  test_configurator()->SetLoopCount(2);

  component_updater()->Start();
  message_loop_.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->install_count());

  EXPECT_EQ(3, interceptor.GetHitCount());
  EXPECT_EQ(1, ping_checker.NumHits()) << ping_checker.GetPings();
  EXPECT_EQ(0, ping_checker.NumMisses()) << ping_checker.GetPings();

  component_updater()->Stop();
}

// This test checks that the "prodversionmin" value is handled correctly. In
// particular there should not be an install because the minimum product
// version is much higher than of chrome.
TEST_F(ComponentUpdaterTest, ProdVersionCheck) {
  std::map<std::string, std::string> map;
  PingChecker ping_checker(map);
  URLRequestPostInterceptor post_interceptor(&ping_checker);
  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  TestInstaller installer;
  CrxComponent com;
  RegisterComponent(&com, kTestComponent_jebg, Version("0.9"), &installer);

  const GURL expected_update_url(
      "http://localhost/upd?extra=foo&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26fp%3D%26uc");

  interceptor.SetResponse(expected_update_url,
                          test_file("updatecheck_reply_2.xml"));
  interceptor.SetResponse(GURL(expected_crx_url),
                          test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  message_loop_.Run();

  EXPECT_EQ(0, ping_checker.NumHits()) << ping_checker.GetPings();
  EXPECT_EQ(0, ping_checker.NumMisses()) << ping_checker.GetPings();
  EXPECT_EQ(1, interceptor.GetHitCount());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  component_updater()->Stop();
}

// Test that a ping for an update check can cause installs.
// Here is the timeline:
//  - First loop: we return a reply that indicates no update, so
//    nothing happens.
//  - We ping.
//  - This triggers a second loop, which has a reply that triggers an install.
TEST_F(ComponentUpdaterTest, CheckForUpdateSoon) {
  std::map<std::string, std::string> map;
  map.insert(std::pair<std::string, std::string>("eventtype", "\"3\""));
  map.insert(std::pair<std::string, std::string>("eventresult", "\"1\""));
  map.insert(std::pair<std::string, std::string>("previousversion",
                                                 "\"0.9\""));
  map.insert(std::pair<std::string, std::string>("nextversion", "\"1.0\""));
  PingChecker ping_checker(map);
  URLRequestPostInterceptor post_interceptor(&ping_checker);
  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  MockComponentObserver observer1;
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  MockComponentObserver observer2;
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_FOUND, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_READY, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  TestInstaller installer1;
  CrxComponent com1;
  com1.observer = &observer1;
  RegisterComponent(&com1, kTestComponent_abag, Version("2.2"), &installer1);
  TestInstaller installer2;
  CrxComponent com2;
  com2.observer = &observer2;
  RegisterComponent(&com2, kTestComponent_jebg, Version("0.9"), &installer2);

  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dabagagagagagagagagagagagagagagag%26v%3D2.2%26fp%3D%26uc"
      "&x=id%3Djebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26fp%3D%26uc");

  const GURL expected_update_url_2(
      "http://localhost/upd?extra=foo"
      "&x=id%3Djebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26fp%3D%26uc"
      "%26installsource%3Dondemand"
      "&x=id%3Dabagagagagagagagagagagagagagagag%26v%3D2.2%26fp%3D%26uc");

  interceptor.SetResponse(expected_update_url_1,
                          test_file("updatecheck_reply_empty"));
  interceptor.SetResponse(expected_update_url_2,
                          test_file("updatecheck_reply_1.xml"));
  interceptor.SetResponse(GURL(expected_crx_url),
                          test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));
  // Test success.
  test_configurator()->SetLoopCount(2);
  test_configurator()->AddComponentToCheck(&com2, 1);
  component_updater()->Start();
  message_loop_.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com2.installer)->install_count());

  EXPECT_EQ(3, interceptor.GetHitCount());

  // Also check what happens if previous check too soon.
  test_configurator()->SetOnDemandTime(60 * 60);
  EXPECT_EQ(ComponentUpdateService::kError,
            component_updater()->CheckForUpdateSoon(com2));
  // Okay, now reset to 0 for the other tests.
  test_configurator()->SetOnDemandTime(0);
  component_updater()->Stop();

  // Test a few error cases. NOTE: We don't have callbacks for
  // when the updates failed yet.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer1));
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer2));
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  const GURL expected_update_url_3(
      "http://localhost/upd?extra=foo"
      "&x=id%3Djebgalgnebhfojomionfpkfelancnnkf%26v%3D1.0%26fp%3D%26uc"
      "&x=id%3Dabagagagagagagagagagagagagagagag%26v%3D2.2%26fp%3D%26uc");

  // No update: error from no server response
  interceptor.SetResponse(expected_update_url_3,
                          test_file("updatecheck_reply_empty"));
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  EXPECT_EQ(ComponentUpdateService::kOk,
            component_updater()->CheckForUpdateSoon(com2));

  message_loop_.Run();

  component_updater()->Stop();

  // No update: already updated to 1.0 so nothing new
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer1));
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer2));
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  interceptor.SetResponse(expected_update_url_3,
                          test_file("updatecheck_reply_1.xml"));
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  EXPECT_EQ(ComponentUpdateService::kOk,
            component_updater()->CheckForUpdateSoon(com2));

  message_loop_.Run();

  EXPECT_EQ(1, ping_checker.NumHits()) << ping_checker.GetPings();
  EXPECT_EQ(0, ping_checker.NumMisses()) << ping_checker.GetPings();

  component_updater()->Stop();
}

// Verify that a previously registered component can get re-registered
// with a different version.
TEST_F(ComponentUpdaterTest, CheckReRegistration) {
  std::map<std::string, std::string> map;
  map.insert(std::pair<std::string, std::string>("eventtype", "\"3\""));
  map.insert(std::pair<std::string, std::string>("eventresult", "\"1\""));
  map.insert(std::pair<std::string, std::string>("previousversion",
                                                 "\"0.9\""));
  map.insert(std::pair<std::string, std::string>("nextversion", "\"1.0\""));
  PingChecker ping_checker(map);
  URLRequestPostInterceptor post_interceptor(&ping_checker);
  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  MockComponentObserver observer1;
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_FOUND, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_READY, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  MockComponentObserver observer2;
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  TestInstaller installer1;
  CrxComponent com1;
  com1.observer = &observer1;
  RegisterComponent(&com1, kTestComponent_jebg, Version("0.9"), &installer1);
  TestInstaller installer2;
  CrxComponent com2;
  com2.observer = &observer2;
  RegisterComponent(&com2, kTestComponent_abag, Version("2.2"), &installer2);

  // Start with 0.9, and update to 1.0
  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo"
      "&x=id%3Djebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26fp%3D%26uc"
      "&x=id%3Dabagagagagagagagagagagagagagagag%26v%3D2.2%26fp%3D%26uc");

  const GURL expected_update_url_2(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dabagagagagagagagagagagagagagagag%26v%3D2.2%26fp%3D%26uc"
      "&x=id%3Djebgalgnebhfojomionfpkfelancnnkf%26v%3D1.0%26fp%3D%26uc");

  interceptor.SetResponse(expected_update_url_1,
                          test_file("updatecheck_reply_1.xml"));
  interceptor.SetResponse(expected_update_url_2,
                          test_file("updatecheck_reply_1.xml"));
  interceptor.SetResponse(GURL(expected_crx_url),
                          test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  // Loop twice to issue two checks: (1) with original 0.9 version
  // and (2) with the updated 1.0 version.
  test_configurator()->SetLoopCount(2);

  component_updater()->Start();
  message_loop_.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->install_count());

  EXPECT_EQ(1, ping_checker.NumHits()) << ping_checker.GetPings();
  EXPECT_EQ(0, ping_checker.NumMisses()) << ping_checker.GetPings();
  EXPECT_EQ(3, interceptor.GetHitCount());

  component_updater()->Stop();

  // Now re-register, pretending to be an even newer version (2.2)
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer1));
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer2));
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  TestInstaller installer3;
  EXPECT_EQ(ComponentUpdateService::kReplaced,
            RegisterComponent(&com1,
                              kTestComponent_jebg,
                              Version("2.2"),
                              &installer3));

  // Check that we send out 2.2 as our version.
  // Interceptor's hit count should go up by 1.
  const GURL expected_update_url_3(
      "http://localhost/upd?extra=foo"
      "&x=id%3Djebgalgnebhfojomionfpkfelancnnkf%26v%3D2.2%26fp%3D%26uc"
      "&x=id%3Dabagagagagagagagagagagagagagagag%26v%3D2.2%26fp%3D%26uc");

  interceptor.SetResponse(expected_update_url_3,
                          test_file("updatecheck_reply_1.xml"));

  // Loop once just to notice the check happening with the re-register version.
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  message_loop_.Run();

  EXPECT_EQ(4, interceptor.GetHitCount());

  // We created a new installer, so the counts go back to 0.
  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->install_count());

  component_updater()->Stop();
}

// Verify that we can download and install a component and a differential
// update to that component. We do three loops; the final loop should do
// nothing.
// We also check that exactly 5 non-ping network requests are issued:
// 1- update check (response: v1 available)
// 2- download crx (v1)
// 3- update check (response: v2 available)
// 4- download differential crx (v1 to v2)
// 5- update check (response: no further update available)
// There should be two pings, one for each update. The second will bear a
// diffresult=1, while the first will not.
TEST_F(ComponentUpdaterTest, DifferentialUpdate) {
  std::map<std::string, std::string> map;
  map.insert(std::pair<std::string, std::string>("eventtype", "\"3\""));
  map.insert(std::pair<std::string, std::string>("eventresult", "\"1\""));
  map.insert(std::pair<std::string, std::string>("diffresult", "\"1\""));
  PingChecker ping_checker(map);
  URLRequestPostInterceptor post_interceptor(&ping_checker);
  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  VersionedTestInstaller installer;
  CrxComponent com;
  RegisterComponent(&com, kTestComponent_ihfo, Version("0.0"), &installer);

  const GURL expected_update_url_0(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dihfokbkgjpifnbbojhneepfflplebdkc%26v%3D0.0%26fp%3D%26uc");
  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dihfokbkgjpifnbbojhneepfflplebdkc%26v%3D1.0%26fp%3D1%26uc");
  const GURL expected_update_url_2(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dihfokbkgjpifnbbojhneepfflplebdkc%26v%3D2.0%26fp%3Df22%26uc");
  const GURL expected_crx_url_1(
      "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx");
  const GURL expected_crx_url_1_diff_2(
      "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx");

  interceptor.SetResponse(expected_update_url_0,
                          test_file("updatecheck_diff_reply_1.xml"));
  interceptor.SetResponse(expected_update_url_1,
                          test_file("updatecheck_diff_reply_2.xml"));
  interceptor.SetResponse(expected_update_url_2,
                          test_file("updatecheck_diff_reply_3.xml"));
  interceptor.SetResponse(expected_crx_url_1,
                          test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"));
  interceptor.SetResponse(
      expected_crx_url_1_diff_2,
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx"));

  test_configurator()->SetLoopCount(3);

  component_updater()->Start();
  message_loop_.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(2, static_cast<TestInstaller*>(com.installer)->install_count());

  // One ping has the diffresult=1, the other does not.
  EXPECT_EQ(1, ping_checker.NumHits()) << ping_checker.GetPings();
  EXPECT_EQ(1, ping_checker.NumMisses()) << ping_checker.GetPings();

  EXPECT_EQ(5, interceptor.GetHitCount());

  component_updater()->Stop();
}

// Verify that component installation falls back to downloading and installing
// a full update if the differential update fails (in this case, because the
// installer does not know about the existing files). We do two loops; the final
// loop should do nothing.
// We also check that exactly 4 non-ping network requests are issued:
// 1- update check (loop 1)
// 2- download differential crx
// 3- download full crx
// 4- update check (loop 2 - no update available)
// There should be one ping for the first attempted update.

TEST_F(ComponentUpdaterTest, DifferentialUpdateFails) {
  std::map<std::string, std::string> map;
  map.insert(std::pair<std::string, std::string>("eventtype", "\"3\""));
  map.insert(std::pair<std::string, std::string>("eventresult", "\"1\""));
  map.insert(std::pair<std::string, std::string>("diffresult", "\"0\""));
  map.insert(std::pair<std::string, std::string>("differrorcode", "\"16\""));
  PingChecker ping_checker(map);
  URLRequestPostInterceptor post_interceptor(&ping_checker);
  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  TestInstaller installer;
  CrxComponent com;
  RegisterComponent(&com, kTestComponent_ihfo, Version("1.0"), &installer);

  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dihfokbkgjpifnbbojhneepfflplebdkc%26v%3D1.0%26fp%3D%26uc");
  const GURL expected_update_url_2(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dihfokbkgjpifnbbojhneepfflplebdkc%26v%3D2.0%26fp%3Df22%26uc");
  const GURL expected_crx_url_1(
      "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx");
  const GURL expected_crx_url_1_diff_2(
      "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx");
  const GURL expected_crx_url_2(
      "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_2.crx");

  interceptor.SetResponse(expected_update_url_1,
                          test_file("updatecheck_diff_reply_2.xml"));
  interceptor.SetResponse(expected_update_url_2,
                          test_file("updatecheck_diff_reply_3.xml"));
  interceptor.SetResponse(expected_crx_url_1,
                          test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"));
  interceptor.SetResponse(
      expected_crx_url_1_diff_2,
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx"));
  interceptor.SetResponse(expected_crx_url_2,
                          test_file("ihfokbkgjpifnbbojhneepfflplebdkc_2.crx"));

  test_configurator()->SetLoopCount(2);

  component_updater()->Start();
  message_loop_.Run();

  // A failed differential update does not count as a failed install.
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com.installer)->install_count());

  EXPECT_EQ(1, ping_checker.NumHits()) << ping_checker.GetPings();
  EXPECT_EQ(0, ping_checker.NumMisses()) << ping_checker.GetPings();
  EXPECT_EQ(4, interceptor.GetHitCount());

  component_updater()->Stop();
}

// Verify that a failed installation causes an install failure ping.
TEST_F(ComponentUpdaterTest, CheckFailedInstallPing) {
  std::map<std::string, std::string> map;
  map.insert(std::pair<std::string, std::string>("eventtype", "\"3\""));
  map.insert(std::pair<std::string, std::string>("eventresult", "\"0\""));
  map.insert(std::pair<std::string, std::string>("errorcode", "\"9\""));
  map.insert(std::pair<std::string, std::string>("previousversion",
                                                 "\"0.9\""));
  map.insert(std::pair<std::string, std::string>("nextversion", "\"1.0\""));
  PingChecker ping_checker(map);
  URLRequestPostInterceptor post_interceptor(&ping_checker);
  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  // This test installer reports installation failure.
  class : public TestInstaller {
    virtual bool Install(const base::DictionaryValue& manifest,
                         const base::FilePath& unpack_path) OVERRIDE {
      ++install_count_;
      base::DeleteFile(unpack_path, true);
      return false;
    }
  } installer;

  CrxComponent com;
  RegisterComponent(&com, kTestComponent_jebg, Version("0.9"), &installer);

  // Start with 0.9, and attempt update to 1.0
  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo"
      "&x=id%3Djebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26fp%3D%26uc");

  interceptor.SetResponse(expected_update_url_1,
                          test_file("updatecheck_reply_1.xml"));
  interceptor.SetResponse(GURL(expected_crx_url),
                          test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  // Loop twice to issue two checks: (1) with original 0.9 version
  // and (2), which should retry with 0.9.
  test_configurator()->SetLoopCount(2);
  component_updater()->Start();
  message_loop_.Run();

  // Loop once more, but expect no ping because a noupdate response is issued.
  // This is necessary to clear out the fire-and-forget ping from the previous
  // iteration.
  interceptor.SetResponse(expected_update_url_1,
                          test_file("updatecheck_reply_noupdate.xml"));
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  message_loop_.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(2, static_cast<TestInstaller*>(com.installer)->install_count());

  EXPECT_EQ(2, ping_checker.NumHits()) << ping_checker.GetPings();
  EXPECT_EQ(0, ping_checker.NumMisses()) << ping_checker.GetPings();
  EXPECT_EQ(5, interceptor.GetHitCount());

  component_updater()->Stop();
}

// Verify that we successfully propagate a patcher error.
// ihfokbkgjpifnbbojhneepfflplebdkc_1to2_bad.crx contains an incorrect
// patching instruction that should fail.
TEST_F(ComponentUpdaterTest, DifferentialUpdateFailErrorcode) {
  std::map<std::string, std::string> map;
  map.insert(std::pair<std::string, std::string>("eventtype", "\"3\""));
  map.insert(std::pair<std::string, std::string>("eventresult", "\"1\""));
  map.insert(std::pair<std::string, std::string>("diffresult", "\"0\""));
  map.insert(std::pair<std::string, std::string>("differrorcode", "\"14\""));
  map.insert(std::pair<std::string, std::string>("diffextracode1", "\"305\""));
  PingChecker ping_checker(map);
  URLRequestPostInterceptor post_interceptor(&ping_checker);
  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  VersionedTestInstaller installer;
  CrxComponent com;
  RegisterComponent(&com, kTestComponent_ihfo, Version("0.0"), &installer);

  const GURL expected_update_url_0(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dihfokbkgjpifnbbojhneepfflplebdkc%26v%3D0.0%26fp%3D%26uc");
  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dihfokbkgjpifnbbojhneepfflplebdkc%26v%3D1.0%26fp%3D1%26uc");
  const GURL expected_update_url_2(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dihfokbkgjpifnbbojhneepfflplebdkc%26v%3D2.0%26fp%3Df22%26uc");
  const GURL expected_crx_url_1(
      "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx");
  const GURL expected_crx_url_1_diff_2(
      "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx");
  const GURL expected_crx_url_2(
      "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_2.crx");

  interceptor.SetResponse(expected_update_url_0,
                          test_file("updatecheck_diff_reply_1.xml"));
  interceptor.SetResponse(expected_update_url_1,
                          test_file("updatecheck_diff_reply_2.xml"));
  interceptor.SetResponse(expected_update_url_2,
                          test_file("updatecheck_diff_reply_3.xml"));
  interceptor.SetResponse(expected_crx_url_1,
                          test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"));
  interceptor.SetResponse(
      expected_crx_url_1_diff_2,
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1to2_bad.crx"));
  interceptor.SetResponse(expected_crx_url_2,
                          test_file("ihfokbkgjpifnbbojhneepfflplebdkc_2.crx"));

  test_configurator()->SetLoopCount(3);

  component_updater()->Start();
  message_loop_.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(2, static_cast<TestInstaller*>(com.installer)->install_count());

  EXPECT_EQ(1, ping_checker.NumHits()) << ping_checker.GetPings();
  EXPECT_EQ(1, ping_checker.NumMisses()) << ping_checker.GetPings();
  EXPECT_EQ(6, interceptor.GetHitCount());

  component_updater()->Stop();
}
