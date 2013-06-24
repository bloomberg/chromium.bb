// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <utility>
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/component_updater/test/component_patcher_mock.h"
#include "chrome/browser/component_updater/test/component_updater_service_unittest.h"
#include "chrome/browser/component_updater/test/test_installer.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_notification_tracker.h"
#include "content/test/net/url_request_prepackaged_interceptor.h"
#include "googleurl/src/gurl.h"
#include "libxml/globals.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::TestNotificationTracker;

TestConfigurator::TestConfigurator()
    : times_(1), recheck_time_(0), ondemand_time_(0), cus_(NULL) {
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

int TestConfigurator::MinimumReCheckWait() {
  return recheck_time_;
}

int TestConfigurator::OnDemandDelay() {
  return ondemand_time_;
}

GURL TestConfigurator::UpdateUrl(CrxComponent::UrlSource source) {
  switch (source) {
    case CrxComponent::BANDAID:
      return GURL("http://localhost/upd");
    case CrxComponent::CWS_PUBLIC:
      return GURL("http://localhost/cws");
    default:
      return GURL("http://wronghost/bad");
  };
}

const char* TestConfigurator::ExtraRequestParams() { return "extra=foo"; }

size_t TestConfigurator::UrlSizeLimit() { return 256; }

net::URLRequestContextGetter* TestConfigurator::RequestContext() {
  return new net::TestURLRequestContextGetter(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
}

// Don't use the utility process to decode files.
bool TestConfigurator::InProcess() { return true; }

void TestConfigurator::OnEvent(Events event, int extra) { }

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

ComponentUpdaterTest::ComponentUpdaterTest() : test_config_(NULL) {
  // The component updater instance under test.
  test_config_ = new TestConfigurator;
  component_updater_.reset(ComponentUpdateServiceFactory(test_config_));
  test_config_->SetComponentUpdateService(component_updater_.get());
  // The test directory is chrome/test/data/components.
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("components");

  // Subscribe to all component updater notifications.
  const int notifications[] = {
    chrome::NOTIFICATION_COMPONENT_UPDATER_STARTED,
    chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING,
    chrome::NOTIFICATION_COMPONENT_UPDATE_FOUND,
    chrome::NOTIFICATION_COMPONENT_UPDATE_READY
  };

  for (int ix = 0; ix != arraysize(notifications); ++ix) {
    notification_tracker_.ListenFor(
        notifications[ix], content::NotificationService::AllSources());
  }
  net::URLFetcher::SetEnableInterceptionForTests(true);
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

TestNotificationTracker& ComponentUpdaterTest::notification_tracker() {
  return notification_tracker_;
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

// Verify that our test fixture work and the component updater can
// be created and destroyed with no side effects.
TEST_F(ComponentUpdaterTest, VerifyFixture) {
  EXPECT_TRUE(component_updater() != NULL);
  EXPECT_EQ(0ul, notification_tracker().size());
}

// Verify that the component updater can be caught in a quick
// start-shutdown situation. Failure of this test will be a crash. Also
// if there is no work to do, there are no notifications generated.
TEST_F(ComponentUpdaterTest, StartStop) {
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);

  component_updater()->Start();
  message_loop.RunUntilIdle();
  component_updater()->Stop();

  EXPECT_EQ(0ul, notification_tracker().size());
}

// Verify that when the server has no updates, we go back to sleep and
// the COMPONENT_UPDATER_STARTED and COMPONENT_UPDATER_SLEEPING notifications
// are generated.
TEST_F(ComponentUpdaterTest, CheckCrxSleep) {
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  TestInstaller installer;
  CrxComponent com;
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
  component_updater()->Start();

  ASSERT_EQ(1ul, notification_tracker().size());
  TestNotificationTracker::Event ev1 = notification_tracker().at(0);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_STARTED, ev1.type);

  message_loop.Run();

  ASSERT_EQ(3ul, notification_tracker().size());
  TestNotificationTracker::Event ev2 = notification_tracker().at(1);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev2.type);
  TestNotificationTracker::Event ev3 = notification_tracker().at(2);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev2.type);
  EXPECT_EQ(2, interceptor.GetHitCount());

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  component_updater()->Stop();

  // Loop twice again but this case we simulate a server error by returning
  // an empty file.

  interceptor.SetResponse(expected_update_url,
                          test_file("updatecheck_reply_empty"));

  notification_tracker().Reset();
  test_configurator()->SetLoopCount(2);
  component_updater()->Start();

  message_loop.Run();

  ASSERT_EQ(3ul, notification_tracker().size());
  ev1 = notification_tracker().at(0);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_STARTED, ev1.type);
  ev2 = notification_tracker().at(1);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev2.type);
  ev3 = notification_tracker().at(2);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev2.type);
  EXPECT_EQ(4, interceptor.GetHitCount());

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  component_updater()->Stop();
}

// Verify that we can check for updates and install one component. Besides
// the notifications above NOTIFICATION_COMPONENT_UPDATE_FOUND and
// NOTIFICATION_COMPONENT_UPDATE_READY should have been fired. We do two loops
// so the second time around there should be nothing left to do.
// We also check that only 3 network requests are issued:
// 1- manifest check
// 2- download crx
// 3- second manifest check.
TEST_F(ComponentUpdaterTest, InstallCrx) {
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  TestInstaller installer1;
  CrxComponent com1;
  RegisterComponent(&com1, kTestComponent_jebg, Version("0.9"), &installer1);
  TestInstaller installer2;
  CrxComponent com2;
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
  message_loop.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->install_count());

  EXPECT_EQ(3, interceptor.GetHitCount());

  ASSERT_EQ(5ul, notification_tracker().size());

  TestNotificationTracker::Event ev1 = notification_tracker().at(1);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATE_FOUND, ev1.type);

  TestNotificationTracker::Event ev2 = notification_tracker().at(2);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATE_READY, ev2.type);

  TestNotificationTracker::Event ev3 = notification_tracker().at(3);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev3.type);

  TestNotificationTracker::Event ev4 = notification_tracker().at(4);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev4.type);

  component_updater()->Stop();
}

// This test is like the above InstallCrx but the second component
// has a different source. In this case there would be two manifest
// checks to different urls, each only containing one component.
TEST_F(ComponentUpdaterTest, InstallCrxTwoSources) {
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  TestInstaller installer1;
  CrxComponent com1;
  RegisterComponent(&com1, kTestComponent_abag, Version("2.2"), &installer1);
  TestInstaller installer2;
  CrxComponent com2;
  com2.source = CrxComponent::CWS_PUBLIC;
  RegisterComponent(&com2, kTestComponent_jebg, Version("0.9"), &installer2);

  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo&x=id%3D"
      "abagagagagagagagagagagagagagagag%26v%3D2.2%26fp%3D%26uc");

  const GURL expected_update_url_2(
      "http://localhost/cws?extra=foo&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26fp%3D%26uc");

  interceptor.SetResponse(expected_update_url_1,
                          test_file("updatecheck_reply_3.xml"));
  interceptor.SetResponse(expected_update_url_2,
                          test_file("updatecheck_reply_1.xml"));
  interceptor.SetResponse(GURL(expected_crx_url),
                          test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  test_configurator()->SetLoopCount(3);

  // We have to set SetRecheckTime to something bigger than 0 or else the
  // component updater will keep re-checking the 'abag' component because
  // the default source pre-empts the other sources.
  test_configurator()->SetRecheckTime(60*60);

  component_updater()->Start();
  message_loop.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com2.installer)->install_count());

  EXPECT_EQ(3, interceptor.GetHitCount());

  ASSERT_EQ(6ul, notification_tracker().size());

  TestNotificationTracker::Event ev0 = notification_tracker().at(1);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev0.type);

  TestNotificationTracker::Event ev1 = notification_tracker().at(2);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATE_FOUND, ev1.type);

  TestNotificationTracker::Event ev2 = notification_tracker().at(3);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATE_READY, ev2.type);

  TestNotificationTracker::Event ev3 = notification_tracker().at(4);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev3.type);

  TestNotificationTracker::Event ev4 = notification_tracker().at(5);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev4.type);

  component_updater()->Stop();
}

// This test checks that the "prodversionmin" value is handled correctly. In
// particular there should not be an install because the minimum product
// version is much higher than of chrome.
TEST_F(ComponentUpdaterTest, ProdVersionCheck) {
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

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
  message_loop.Run();

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
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  TestInstaller installer1;
  CrxComponent com1;
  RegisterComponent(&com1, kTestComponent_abag, Version("2.2"), &installer1);
  TestInstaller installer2;
  CrxComponent com2;
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
  message_loop.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com2.installer)->install_count());

  EXPECT_EQ(3, interceptor.GetHitCount());

  ASSERT_EQ(5ul, notification_tracker().size());

  TestNotificationTracker::Event ev0= notification_tracker().at(0);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_STARTED, ev0.type);

  TestNotificationTracker::Event ev1 = notification_tracker().at(1);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev1.type);

  TestNotificationTracker::Event ev2 = notification_tracker().at(2);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATE_FOUND, ev2.type);

  TestNotificationTracker::Event ev3 = notification_tracker().at(3);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATE_READY, ev3.type);

  TestNotificationTracker::Event ev4 = notification_tracker().at(4);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev4.type);

  // Also check what happens if previous check too soon.
  test_configurator()->SetOnDemandTime(60 * 60);
  EXPECT_EQ(ComponentUpdateService::kError,
            component_updater()->CheckForUpdateSoon(com2));
  // Okay, now reset to 0 for the other tests.
  test_configurator()->SetOnDemandTime(0);
  component_updater()->Stop();

  // Test a few error cases. NOTE: We don't have callbacks for
  // when the updates failed yet.
  const GURL expected_update_url_3(
      "http://localhost/upd?extra=foo"
      "&x=id%3Djebgalgnebhfojomionfpkfelancnnkf%26v%3D1.0%26fp%3D%26uc"
      "&x=id%3Dabagagagagagagagagagagagagagagag%26v%3D2.2%26fp%3D%26uc");

  // No update: error from no server response
  interceptor.SetResponse(expected_update_url_3,
                          test_file("updatecheck_reply_empty"));
  notification_tracker().Reset();
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  EXPECT_EQ(ComponentUpdateService::kOk,
            component_updater()->CheckForUpdateSoon(com2));

  message_loop.Run();

  ASSERT_EQ(2ul, notification_tracker().size());
  ev0 = notification_tracker().at(0);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_STARTED, ev0.type);
  ev1 = notification_tracker().at(1);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev1.type);
  component_updater()->Stop();

  // No update: already updated to 1.0 so nothing new
  interceptor.SetResponse(expected_update_url_3,
                          test_file("updatecheck_reply_1.xml"));
  notification_tracker().Reset();
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  EXPECT_EQ(ComponentUpdateService::kOk,
            component_updater()->CheckForUpdateSoon(com2));

  message_loop.Run();

  ASSERT_EQ(2ul, notification_tracker().size());
  ev0 = notification_tracker().at(0);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_STARTED, ev0.type);
  ev1 = notification_tracker().at(1);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev1.type);
  component_updater()->Stop();
}

// Verify that a previously registered component can get re-registered
// with a different version.
TEST_F(ComponentUpdaterTest, CheckReRegistration) {
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  TestInstaller installer1;
  CrxComponent com1;
  RegisterComponent(&com1, kTestComponent_jebg, Version("0.9"), &installer1);
  TestInstaller installer2;
  CrxComponent com2;
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
  message_loop.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->install_count());

  EXPECT_EQ(3, interceptor.GetHitCount());

  ASSERT_EQ(5ul, notification_tracker().size());

  TestNotificationTracker::Event ev0 = notification_tracker().at(0);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_STARTED, ev0.type);

  TestNotificationTracker::Event ev1 = notification_tracker().at(1);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATE_FOUND, ev1.type);

  TestNotificationTracker::Event ev2 = notification_tracker().at(2);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATE_READY, ev2.type);

  TestNotificationTracker::Event ev3 = notification_tracker().at(3);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev3.type);

  TestNotificationTracker::Event ev4 = notification_tracker().at(4);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev4.type);

  // Now re-register, pretending to be an even newer version (2.2)
  TestInstaller installer3;
  component_updater()->Stop();
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

  notification_tracker().Reset();

  // Loop once just to notice the check happening with the re-register version.
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  message_loop.Run();

  ASSERT_EQ(2ul, notification_tracker().size());

  ev0 = notification_tracker().at(0);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_STARTED, ev0.type);

  ev1 = notification_tracker().at(1);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev1.type);

  EXPECT_EQ(4, interceptor.GetHitCount());

  // We created a new installer, so the counts go back to 0.
  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->install_count());

  component_updater()->Stop();
}

// Verify that component installation falls back to downloading and installing
// a full update if the differential update fails (in this case, because the
// installer does not know about the existing files). We do two loops; the final
// loop should do nothing.
// We also check that exactly 4 network requests are issued:
// 1- update check (loop 1)
// 2- download differential crx
// 3- download full crx
// 4- update check (loop 2 - no update available)
TEST_F(ComponentUpdaterTest, DifferentialUpdateFails) {
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

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
  message_loop.Run();

  // A failed differential update does not count as a failed install.
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com.installer)->install_count());

  EXPECT_EQ(4, interceptor.GetHitCount());

  component_updater()->Stop();
}

// Verify that we successfully propagate a patcher error.
// ihfokbkgjpifnbbojhneepfflplebdkc_1to2_bad.crx contains an incorrect
// patching instruction that should fail.
TEST_F(ComponentUpdaterTest, DifferentialUpdateFailErrorcode) {
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

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
  message_loop.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(2, static_cast<TestInstaller*>(com.installer)->install_count());

  EXPECT_EQ(6, interceptor.GetHitCount());

  component_updater()->Stop();
}
