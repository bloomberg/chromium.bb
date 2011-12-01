// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_service.h"

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/component_updater/component_updater_interceptor.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/test_url_request_context_getter.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "content/test/test_notification_tracker.h"

#include "googleurl/src/gurl.h"
#include "libxml/globals.h"

#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {
// Overrides some of the component updater behaviors so it is easier to test
// and loops faster. In actual usage it takes hours do to a full cycle.
class TestConfigurator : public ComponentUpdateService::Configurator {
 public:
  TestConfigurator() : times_(1) {
  }

  virtual int InitialDelay() OVERRIDE { return 0; }

  virtual int NextCheckDelay() OVERRIDE {
    // This is called when a new full cycle of checking for updates is going
    // to happen. In test we normally only test one cycle so it is a good
    // time to break from the test messageloop Run() method so the test can
    // finish.
    if (--times_ > 0)
      return 1;

    MessageLoop::current()->Quit();
    return 0;
  }

  virtual int StepDelay() OVERRIDE {
    return 0;
  }

  virtual int MinimumReCheckWait() OVERRIDE {
    return 0;
  }

  virtual GURL UpdateUrl() OVERRIDE { return GURL("http://localhost/upd"); }

  virtual const char* ExtraRequestParams() OVERRIDE { return "extra=foo"; }

  virtual size_t UrlSizeLimit() OVERRIDE { return 256; }

  virtual net::URLRequestContextGetter* RequestContext() OVERRIDE {
    return new TestURLRequestContextGetter();
  }

  // Don't use the utility process to decode files.
  virtual bool InProcess() OVERRIDE { return true; }

  virtual void OnEvent(Events event, int extra) OVERRIDE { }

  // Set how many update checks are called, the default value is just once.
  void SetLoopCount(int times) { times_ = times; }

 private:
  int times_;
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
                       const FilePath& unpack_path) OVERRIDE {
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

const char header_ok_reply[] =
    "HTTP/1.1 200 OK\0"
    "Content-type: text/html\0"
    "\0";

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
    content::URLFetcher::SetEnableInterceptionForTests(true);
  }

  ~ComponentUpdaterTest() {
    content::URLFetcher::SetEnableInterceptionForTests(false);
  }

  ComponentUpdateService* component_updater() {
    return component_updater_.get();
  }

  // Makes the full path to a component updater test file.
  const FilePath test_file(const char* file) {
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
    com->installer = new TestInstaller;
    component_updater_->RegisterComponent(*com);
  }

 private:
  scoped_ptr<ComponentUpdateService> component_updater_;
  FilePath test_data_dir_;
  TestNotificationTracker notification_tracker_;
  TestConfigurator* test_config_;
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
  message_loop.RunAllPending();
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

  scoped_refptr<ComponentUpdateInterceptor>
      interceptor(new ComponentUpdateInterceptor());

  CrxComponent com;
  RegisterComponent(&com, kTestComponent_abag, Version("1.1"));

  const char expected_update_url[] =
      "http://localhost/upd?extra=foo&x=id%3D"
      "abagagagagagagagagagagagagagagag%26v%3D1.1%26uc";

  interceptor->SetResponse(expected_update_url,
                           header_ok_reply,
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
  EXPECT_EQ(2, interceptor->hit_count());

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  component_updater()->Stop();

  // Loop twice again but this case we simulate a server error by returning
  // an empty file.

  interceptor->SetResponse(expected_update_url,
                           header_ok_reply,
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
  EXPECT_EQ(4, interceptor->hit_count());

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  component_updater()->Stop();

  delete com.installer;
  xmlCleanupGlobals();
}

// Verify that we can check for updates and install one component. Besides
// the notifications above NOTIFICATION_COMPONENT_UPDATE_FOUND and
// NOTIFICATION_COMPONENT_UPDATE_READY should have been fired. We do two loops
// so the second time arround there should be nothing left to do.
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

  scoped_refptr<ComponentUpdateInterceptor>
      interceptor(new ComponentUpdateInterceptor());

  CrxComponent com1;
  RegisterComponent(&com1, kTestComponent_jebg, Version("0.9"));
  CrxComponent com2;
  RegisterComponent(&com2, kTestComponent_abag, Version("2.2"));

  const char expected_update_url_1[] =
      "http://localhost/upd?extra=foo&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26uc&x=id%3D"
      "abagagagagagagagagagagagagagagag%26v%3D2.2%26uc";

  const char expected_update_url_2[] =
      "http://localhost/upd?extra=foo&x=id%3D"
      "abagagagagagagagagagagagagagagag%26v%3D2.2%26uc&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D1.0%26uc";

  interceptor->SetResponse(expected_update_url_1, header_ok_reply,
                           test_file("updatecheck_reply_1.xml"));
  interceptor->SetResponse(expected_update_url_2, header_ok_reply,
                           test_file("updatecheck_reply_1.xml"));
  interceptor->SetResponse(expected_crx_url, header_ok_reply,
                           test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  test_configurator()->SetLoopCount(2);

  component_updater()->Start();
  message_loop.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(3, interceptor->hit_count());

  ASSERT_EQ(5ul, notification_tracker().size());

  TestNotificationTracker::Event ev1 = notification_tracker().at(1);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATE_FOUND, ev1.type);

  TestNotificationTracker::Event ev2 = notification_tracker().at(2);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATE_READY, ev2.type);

  TestNotificationTracker::Event ev3 = notification_tracker().at(3);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev3.type);

  TestNotificationTracker::Event ev4 = notification_tracker().at(4);
  EXPECT_EQ(chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING, ev3.type);

  component_updater()->Stop();
  delete com1.installer;
  delete com2.installer;
  xmlCleanupGlobals();
}

// This test checks that the "prodversionmin" value is handled correctly. In
// particular there should not be an install because the minimun product
// version is much higher than of chrome.
TEST_F(ComponentUpdaterTest, ProdVersionCheck) {
  MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  scoped_refptr<ComponentUpdateInterceptor>
      interceptor(new ComponentUpdateInterceptor());

  CrxComponent com;
  RegisterComponent(&com, kTestComponent_jebg, Version("0.9"));

  const char expected_update_url[] =
      "http://localhost/upd?extra=foo&x=id%3D"
      "jebgalgnebhfojomionfpkfelancnnkf%26v%3D0.9%26uc";

  interceptor->SetResponse(expected_update_url,
                           header_ok_reply,
                           test_file("updatecheck_reply_2.xml"));
  interceptor->SetResponse(expected_crx_url, header_ok_reply,
                           test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  message_loop.Run();

  EXPECT_EQ(1, interceptor->hit_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  component_updater()->Stop();
}
