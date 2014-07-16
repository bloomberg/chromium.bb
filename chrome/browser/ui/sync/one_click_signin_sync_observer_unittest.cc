// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_sync_observer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/startup_controller.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

const char kContinueUrl[] = "https://www.example.com/";

class MockWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit MockWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  virtual ~MockWebContentsObserver() {}

  // A hook to verify that the OneClickSigninSyncObserver initiated a redirect
  // to the continue URL. Navigations in unit_tests never complete, but a
  // navigation start is a sufficient signal for the purposes of this test.
  // Listening for this call also has the advantage of being synchronous.
  MOCK_METHOD1(AboutToNavigateRenderView, void(content::RenderViewHost*));
};

class OneClickTestProfileSyncService : public TestProfileSyncService {
 public:
  virtual ~OneClickTestProfileSyncService() {}

  // Helper routine to be used in conjunction with
  // BrowserContextKeyedServiceFactory::SetTestingFactory().
  static KeyedService* Build(content::BrowserContext* profile) {
    return new OneClickTestProfileSyncService(static_cast<Profile*>(profile));
  }

  virtual bool FirstSetupInProgress() const OVERRIDE {
    return first_setup_in_progress_;
  }

  virtual bool sync_initialized() const OVERRIDE { return sync_initialized_; }

  void set_first_setup_in_progress(bool in_progress) {
    first_setup_in_progress_ = in_progress;
  }

  void set_sync_initialized(bool initialized) {
    sync_initialized_ = initialized;
  }

 private:
  explicit OneClickTestProfileSyncService(Profile* profile)
      : TestProfileSyncService(
          scoped_ptr<ProfileSyncComponentsFactory>(
              new ProfileSyncComponentsFactoryMock()),
          profile,
          SigninManagerFactory::GetForProfile(profile),
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          browser_sync::MANUAL_START),
        first_setup_in_progress_(false),
        sync_initialized_(false) {}

  bool first_setup_in_progress_;
  bool sync_initialized_;
};

class TestOneClickSigninSyncObserver : public OneClickSigninSyncObserver {
 public:
  typedef base::Callback<void(TestOneClickSigninSyncObserver*)>
      DestructionCallback;

  TestOneClickSigninSyncObserver(content::WebContents* web_contents,
                                 const GURL& continue_url,
                                 const DestructionCallback& callback)
      : OneClickSigninSyncObserver(web_contents, continue_url),
        destruction_callback_(callback) {}
  virtual ~TestOneClickSigninSyncObserver() { destruction_callback_.Run(this); }

 private:
  DestructionCallback destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestOneClickSigninSyncObserver);
};

// A trivial factory to build a null service.
KeyedService* BuildNullService(content::BrowserContext* context) {
  return NULL;
}

}  // namespace

class OneClickSigninSyncObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  OneClickSigninSyncObserverTest()
      : sync_service_(NULL),
        sync_observer_(NULL),
        sync_observer_destroyed_(true) {}

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents_observer_.reset(new MockWebContentsObserver(web_contents()));
    sync_service_ =
        static_cast<OneClickTestProfileSyncService*>(
            ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
                profile(), OneClickTestProfileSyncService::Build));
  }

  virtual void TearDown() OVERRIDE {
    // Verify that the |sync_observer_| unregistered as an observer from the
    // sync service and freed its memory.
    EXPECT_TRUE(sync_observer_destroyed_);
    if (sync_service_)
      EXPECT_FALSE(sync_service_->HasObserver(sync_observer_));
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void CreateSyncObserver(const std::string& url) {
    sync_observer_ = new TestOneClickSigninSyncObserver(
      web_contents(), GURL(url),
      base::Bind(&OneClickSigninSyncObserverTest::OnSyncObserverDestroyed,
                 base::Unretained(this)));
    if (sync_service_)
      EXPECT_TRUE(sync_service_->HasObserver(sync_observer_));
    EXPECT_TRUE(sync_observer_destroyed_);
    sync_observer_destroyed_ = false;
  }

  OneClickTestProfileSyncService* sync_service_;
  scoped_ptr<MockWebContentsObserver> web_contents_observer_;

 private:
  void OnSyncObserverDestroyed(TestOneClickSigninSyncObserver* observer) {
    EXPECT_EQ(sync_observer_, observer);
    EXPECT_FALSE(sync_observer_destroyed_);
    sync_observer_destroyed_ = true;
  }

  TestOneClickSigninSyncObserver* sync_observer_;
  bool sync_observer_destroyed_;
};

// Verify that if no Sync service is present, e.g. because Sync is disabled, the
// observer immediately loads the continue URL.
TEST_F(OneClickSigninSyncObserverTest, NoSyncService_RedirectsImmediately) {
  // Simulate disabling Sync.
  sync_service_ =
      static_cast<OneClickTestProfileSyncService*>(
          ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              profile(), BuildNullService));

  // The observer should immediately redirect to the continue URL.
  EXPECT_CALL(*web_contents_observer_, AboutToNavigateRenderView(_));
  CreateSyncObserver(kContinueUrl);
  EXPECT_EQ(GURL(kContinueUrl), web_contents()->GetVisibleURL());

  // The |sync_observer_| will be destroyed asynchronously, so manually pump
  // the message loop to wait for the destruction.
  content::RunAllPendingInMessageLoop();
}

// Verify that when the WebContents is destroyed without any Sync notifications
// firing, the observer cleans up its memory without loading the continue URL.
TEST_F(OneClickSigninSyncObserverTest, WebContentsDestroyed) {
  EXPECT_CALL(*web_contents_observer_, AboutToNavigateRenderView(_)).Times(0);
  CreateSyncObserver(kContinueUrl);
  SetContents(NULL);
}

// Verify that when Sync is configured successfully, the observer loads the
// continue URL and cleans up after itself.
TEST_F(OneClickSigninSyncObserverTest,
       OnSyncStateChanged_SyncConfiguredSuccessfully) {
  CreateSyncObserver(kContinueUrl);
  sync_service_->set_first_setup_in_progress(false);
  sync_service_->set_sync_initialized(true);

  EXPECT_CALL(*web_contents_observer_, AboutToNavigateRenderView(_));
  sync_service_->NotifyObservers();
  EXPECT_EQ(GURL(kContinueUrl), web_contents()->GetVisibleURL());
}

// Verify that when Sync configuration fails, the observer does not load the
// continue URL, but still cleans up after itself.
TEST_F(OneClickSigninSyncObserverTest,
       OnSyncStateChanged_SyncConfigurationFailed) {
  CreateSyncObserver(kContinueUrl);
  sync_service_->set_first_setup_in_progress(false);
  sync_service_->set_sync_initialized(false);

  EXPECT_CALL(*web_contents_observer_, AboutToNavigateRenderView(_)).Times(0);
  sync_service_->NotifyObservers();
  EXPECT_NE(GURL(kContinueUrl), web_contents()->GetVisibleURL());
}

// Verify that when Sync sends a notification while setup is not yet complete,
// the observer does not load the continue URL, and continues to wait.
TEST_F(OneClickSigninSyncObserverTest,
       OnSyncStateChanged_SyncConfigurationInProgress) {
  CreateSyncObserver(kContinueUrl);
  sync_service_->set_first_setup_in_progress(true);
  sync_service_->set_sync_initialized(false);

  EXPECT_CALL(*web_contents_observer_, AboutToNavigateRenderView(_)).Times(0);
  sync_service_->NotifyObservers();
  EXPECT_NE(GURL(kContinueUrl), web_contents()->GetVisibleURL());

  // Trigger an event to force state to be cleaned up.
  SetContents(NULL);
}

// Verify that if the continue_url is to the settings page, no navigation is
// triggered, since it would be redundant.
TEST_F(OneClickSigninSyncObserverTest,
       OnSyncStateChanged_SyncConfiguredSuccessfully_SourceIsSettings) {
  GURL continue_url = signin::GetPromoURL(signin::SOURCE_SETTINGS, false);
  CreateSyncObserver(continue_url.spec());
  sync_service_->set_first_setup_in_progress(false);
  sync_service_->set_sync_initialized(true);

  EXPECT_CALL(*web_contents_observer_, AboutToNavigateRenderView(_)).Times(0);
  sync_service_->NotifyObservers();
  EXPECT_NE(GURL(kContinueUrl), web_contents()->GetVisibleURL());
}
