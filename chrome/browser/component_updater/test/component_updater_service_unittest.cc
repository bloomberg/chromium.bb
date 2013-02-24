// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_service.h"

#include <list>
#include <utility>

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_notification_tracker.h"
#include "content/test/net/url_request_prepackaged_interceptor.h"
#include "googleurl/src/gurl.h"
#include "libxml/globals.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::TestNotificationTracker;

namespace {
// Overrides some of the component updater behaviors so it is easier to test
// and loops faster. In actual usage it takes hours do to a full cycle.
class TestConfigurator : public ComponentUpdateService::Configurator {
 public:
  TestConfigurator()
      : times_(1), recheck_time_(0), ondemand_time_(0), cus_(NULL) {
  }

  virtual int InitialDelay() OVERRIDE { return 0; }

  typedef std::pair<CrxComponent*, int> CheckAtLoopCount;

  virtual int NextCheckDelay() OVERRIDE {
    // This is called when a new full cycle of checking for updates is going
    // to happen. In test we normally only test one cycle so it is a good
    // time to break from the test messageloop Run() method so the test can
    // finish.
    if (--times_ <= 0) {
      MessageLoop::current()->Quit();
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

  virtual int StepDelay() OVERRIDE {
    return 0;
  }

  virtual int MinimumReCheckWait() OVERRIDE {
    return recheck_time_;
  }

  virtual int OnDemandDelay() OVERRIDE {
    return ondemand_time_;
  }

  virtual GURL UpdateUrl(CrxComponent::UrlSource source) OVERRIDE {
    switch (source) {
      case CrxComponent::BANDAID:
        return GURL("http://localhost/upd");
      case CrxComponent::CWS_PUBLIC:
        return GURL("http://localhost/cws");
      default:
        return GURL("http://wronghost/bad");
    };
  }

  virtual const char* ExtraRequestParams() OVERRIDE { return "extra=foo"; }

  virtual size_t UrlSizeLimit() OVERRIDE { return 256; }

  virtual net::URLRequestContextGetter* RequestContext() OVERRIDE {
    return new net::TestURLRequestContextGetter(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  }

  // Don't use the utility process to decode files.
  virtual bool InProcess() OVERRIDE { return true; }

  virtual void OnEvent(Events event, int extra) OVERRIDE { }

  // Set how many update checks are called, the default value is just once.
  void SetLoopCount(int times) { times_ = times; }

  void SetRecheckTime(int seconds) {
    recheck_time_ = seconds;
  }

  void SetOnDemandTime(int seconds) {
    ondemand_time_ = seconds;
  }

  void AddComponentToCheck(CrxComponent* com, int at_loop_iter) {
    components_to_check_.push_back(std::make_pair(com, at_loop_iter));
  }

  void SetComponentUpdateService(ComponentUpdateService* cus) {
    cus_ = cus;
  }

 private:
  int times_;
  int recheck_time_;
  int ondemand_time_;

  std::list<CheckAtLoopCount> components_to_check_;
  ComponentUpdateService* cus_;
};

class TestInstaller : public ComponentInstaller {
 public :
  explicit TestInstaller()
      : error_(0), install_count_(0) {
  }

  virtual void OnUpdateError(int error) OVERRIDE {
    EXPECT_NE(0, error);
    error_ = error;
  }

  virtual bool Install(base::DictionaryValue* manifest,
                       const base::FilePath& unpack_path) OVERRIDE {
    ++install_count_;
    delete manifest;
    return file_util::Delete(unpack_path, true);
  }

  int error() const { return error_; }

  int install_count() const { return install_count_; }

 private:
  int error_;
  int install_count_;
};

// component 1 has extension id "jebgalgnebhfojomionfpkfelancnnkf", and
// the RSA public key the following hash:
const uint8 jebg_hash[] = {0x94,0x16,0x0b,0x6d,0x41,0x75,0xe9,0xec,0x8e,0xd5,
                           0xfa,0x54,0xb0,0xd2,0xdd,0xa5,0x6e,0x05,0x6b,0xe8,
                           0x73,0x47,0xf6,0xc4,0x11,0x9f,0xbc,0xb3,0x09,0xb3,
                           0x5b,0x40};
// component 2 has extension id "abagagagagagagagagagagagagagagag", and
// the RSA public key the following hash:
const uint8 abag_hash[] = {0x01,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
                           0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
                           0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
                           0x06,0x01};

const char expected_crx_url[] =
    "http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx";

}  // namespace

// Common fixture for all the component updater tests.
class ComponentUpdaterTest : public testing::Test {
 public:
  enum TestComponents {
    kTestComponent_abag,
    kTestComponent_jebg
  };

  ComponentUpdaterTest() : component_updater_(NULL), test_config_(NULL) {
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

  virtual ~ComponentUpdaterTest() {
    net::URLFetcher::SetEnableInterceptionForTests(false);
  }

  virtual void TearDown() {
    xmlCleanupGlobals();
  }

  ComponentUpdateService* component_updater() {
    return component_updater_.get();
  }

  // Makes the full path to a component updater test file.
  const base::FilePath test_file(const char* file) {
    return test_data_dir_.AppendASCII(file);
  }

  TestNotificationTracker& notification_tracker() {
    return notification_tracker_;
  }

  TestConfigurator* test_configurator() {
    return test_config_;
  }

  void RegisterComponent(CrxComponent* com,
                         TestComponents component,
                         const Version& version) {
    if (component == kTestComponent_abag) {
      com->name = "test_abag";
      com->pk_hash.assign(abag_hash, abag_hash + arraysize(abag_hash));
    } else {
      com->name = "test_jebg";
      com->pk_hash.assign(jebg_hash, jebg_hash + arraysize(jebg_hash));
    }
    com->version = version;
    TestInstaller* installer = new TestInstaller;
    com->installer = installer;
    test_installers_.push_back(installer);
    component_updater_->RegisterComponent(*com);
  }

 private:
  scoped_ptr<ComponentUpdateService> component_updater_;
  base::FilePath test_data_dir_;
  TestNotificationTracker notification_tracker_;
  TestConfigurator* test_config_;
  // ComponentInstaller objects to delete after each test.
  ScopedVector<TestInstaller> test_installers_;
};

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
  MessageLoop message_loop;
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
  MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  content::URLRequestPrepackagedInterceptor interceptor;

  CrxComponent com;
  RegisterComponent(&com, kTestComponent_abag, Version("1.1"));

  const GURL expected_update_url(
      "http://localhost/upd?extra=foo&x=id%3D"
      "abagagagagagagagagagagagagagagag%26v%3D1.1%26uc");

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
  MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  content::URLRequestPrepackagedInterceptor interceptor;

  CrxComponent com1;
  RegisterComponent(&com1, kTestComponent_jebg, Version("0.9"));
  CrxComponent com2;
  RegisterComponent(&com2, kTestComponent_abag, Version("2.2"));

  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26uc&x=id%3D"
      "abagagagagagagagagagagagagagagag%26v%3D2.2%26uc");

  const GURL expected_update_url_2(
      "http://localhost/upd?extra=foo&x=id%3D"
      "abagagagagagagagagagagagagagagag%26v%3D2.2%26uc&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D1.0%26uc");

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
  MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  content::URLRequestPrepackagedInterceptor interceptor;

  CrxComponent com1;
  RegisterComponent(&com1, kTestComponent_abag, Version("2.2"));
  CrxComponent com2;
  com2.source = CrxComponent::CWS_PUBLIC;
  RegisterComponent(&com2, kTestComponent_jebg, Version("0.9"));

  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo&x=id%3D"
      "abagagagagagagagagagagagagagagag%26v%3D2.2%26uc");

  const GURL expected_update_url_2(
      "http://localhost/cws?extra=foo&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26uc");

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
  MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  content::URLRequestPrepackagedInterceptor interceptor;

  CrxComponent com;
  RegisterComponent(&com, kTestComponent_jebg, Version("0.9"));

  const GURL expected_update_url(
      "http://localhost/upd?extra=foo&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26uc");

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
  MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  content::URLRequestPrepackagedInterceptor interceptor;

  CrxComponent com1;
  RegisterComponent(&com1, kTestComponent_abag, Version("2.2"));
  CrxComponent com2;
  RegisterComponent(&com2, kTestComponent_jebg, Version("0.9"));

  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo&x=id%3D"
      "abagagagagagagagagagagagagagagag%26v%3D2.2%26uc&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26uc");

  const GURL expected_update_url_2(
      "http://localhost/upd?extra=foo&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26uc&x=id%3D"
      "abagagagagagagagagagagagagagagag%26v%3D2.2%26uc");

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
      "http://localhost/upd?extra=foo&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D1.0%26uc&x=id%3D"
      "abagagagagagagagagagagagagagagag%26v%3D2.2%26uc");

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
