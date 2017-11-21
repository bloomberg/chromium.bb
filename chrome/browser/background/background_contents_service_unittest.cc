// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents_service.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/message_center/fake_ui_delegate.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification.h"
#include "url/gurl.h"

class BackgroundContentsServiceTest : public testing::Test {
 public:
  BackgroundContentsServiceTest() {}
  ~BackgroundContentsServiceTest() override {}
  void SetUp() override {
    command_line_.reset(new base::CommandLine(base::CommandLine::NO_PROGRAM));
    BackgroundContentsService::DisableCloseBalloonForTesting(true);
  }

  void TearDown() override {
    BackgroundContentsService::DisableCloseBalloonForTesting(false);
  }

  const base::DictionaryValue* GetPrefs(Profile* profile) {
    return profile->GetPrefs()->GetDictionary(
        prefs::kRegisteredBackgroundContents);
  }

  // Returns the stored pref URL for the passed app id.
  std::string GetPrefURLForApp(Profile* profile, const std::string& appid) {
    const base::DictionaryValue* pref = GetPrefs(profile);
    EXPECT_TRUE(pref->HasKey(appid));
    const base::DictionaryValue* value;
    pref->GetDictionaryWithoutPathExpansion(appid, &value);
    std::string url;
    value->GetString("url", &url);
    return url;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<base::CommandLine> command_line_;
};

class MockBackgroundContents : public BackgroundContents {
 public:
  explicit MockBackgroundContents(Profile* profile)
      : appid_("app_id"), profile_(profile) {}
  MockBackgroundContents(Profile* profile, const std::string& id)
      : appid_(id), profile_(profile) {}

  void SendOpenedNotification(BackgroundContentsService* service) {
    BackgroundContentsOpenedDetails details = {
        this, "background", appid_ };
    service->BackgroundContentsOpened(&details, profile_);
  }

  virtual void Navigate(GURL url) {
    url_ = url;
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
        content::Source<Profile>(profile_),
        content::Details<BackgroundContents>(this));
  }
  const GURL& GetURL() const override { return url_; }

  void MockClose(Profile* profile) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_BACKGROUND_CONTENTS_CLOSED,
        content::Source<Profile>(profile),
        content::Details<BackgroundContents>(this));
    delete this;
  }

  ~MockBackgroundContents() override {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
        content::Source<Profile>(profile_),
        content::Details<BackgroundContents>(this));
  }

  const std::string& appid() { return appid_; }

 private:
  GURL url_;

  // The ID of our parent application
  std::string appid_;

  // Parent profile
  Profile* profile_;
};

// Wait for the notification created.
class NotificationWaiter : public message_center::MessageCenterObserver {
 public:
  explicit NotificationWaiter(const std::string& target_id, Profile* profile)
      : target_id_(target_id), profile_(profile) {}
  ~NotificationWaiter() override {}

  void WaitForNotificationAdded() {
    DCHECK(!run_loop_.running());
    message_center::MessageCenter* message_center =
        message_center::MessageCenter::Get();

    message_center->AddObserver(this);
    run_loop_.Run();
    message_center->RemoveObserver(this);
  }

 private:
  // message_center::MessageCenterObserver overrides:
  void OnNotificationAdded(const std::string& notification_id) override {
    if (notification_id == FindNotificationIdFromDelegateId(target_id_))
      run_loop_.Quit();
  }

  void OnNotificationUpdated(const std::string& notification_id) override {
    if (notification_id == FindNotificationIdFromDelegateId(target_id_))
      run_loop_.Quit();
  }

  std::string FindNotificationIdFromDelegateId(const std::string& delegate_id) {
    MessageCenterNotificationManager* manager =
        static_cast<MessageCenterNotificationManager*>(
            g_browser_process->notification_ui_manager());
    DCHECK(manager);
    return manager
        ->FindById(delegate_id, NotificationUIManager::GetProfileID(profile_))
        ->id();
  }

  std::string target_id_;
  Profile* profile_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(NotificationWaiter);
};

class BackgroundContentsServiceNotificationTest
    : public BrowserWithTestWindowTest {
 public:
  BackgroundContentsServiceNotificationTest() {}
  ~BackgroundContentsServiceNotificationTest() override {}

  // Overridden from testing::Test
  void SetUp() override {
    // In ChromeOS environment, BrowserWithTestWindowTest initializes
    // MessageCenter.
#if !defined(OS_CHROMEOS)
    message_center::MessageCenter::Initialize();
#endif
    BrowserWithTestWindowTest::SetUp();

    MessageCenterNotificationManager* manager =
        static_cast<MessageCenterNotificationManager*>(
            g_browser_process->notification_ui_manager());
    manager->SetUiDelegateForTest(new message_center::FakeUiDelegate());
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
#if !defined(OS_CHROMEOS)
    message_center::MessageCenter::Shutdown();
#endif
  }

 protected:
  // Creates crash notification for the specified extension and returns
  // the created one.
  const message_center::Notification* CreateCrashNotification(
      scoped_refptr<extensions::Extension> extension) {
    std::string notification_id = BackgroundContentsService::
        GetNotificationDelegateIdForExtensionForTesting(extension->id());
    NotificationWaiter waiter(notification_id, profile());
    BackgroundContentsService::ShowBalloonForTesting(
        extension.get(), profile());
    waiter.WaitForNotificationAdded();

    return g_browser_process->notification_ui_manager()->FindById(
        notification_id, profile());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundContentsServiceNotificationTest);
};

TEST_F(BackgroundContentsServiceTest, Create) {
  // Check for creation and leaks.
  TestingProfile profile;
  BackgroundContentsService service(&profile, command_line_.get());
}

TEST_F(BackgroundContentsServiceTest, BackgroundContentsCreateDestroy) {
  TestingProfile profile;
  BackgroundContentsService service(&profile, command_line_.get());
  MockBackgroundContents* contents = new MockBackgroundContents(&profile);
  EXPECT_FALSE(service.IsTracked(contents));
  contents->SendOpenedNotification(&service);
  EXPECT_TRUE(service.IsTracked(contents));
  delete contents;
  EXPECT_FALSE(service.IsTracked(contents));
}

TEST_F(BackgroundContentsServiceTest, BackgroundContentsUrlAdded) {
  TestingProfile profile;
  BackgroundContentsService service(&profile, command_line_.get());
  BackgroundContentsServiceFactory::GetInstance()->
      RegisterUserPrefsOnBrowserContextForTest(&profile);
  GURL orig_url;
  GURL url("http://a/");
  GURL url2("http://a/");
  {
    std::unique_ptr<MockBackgroundContents> contents(
        new MockBackgroundContents(&profile));
    EXPECT_EQ(0U, GetPrefs(&profile)->size());
    contents->SendOpenedNotification(&service);

    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(&profile)->size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(&profile, contents->appid()));

    // Navigate the contents to a new url, should not change url.
    contents->Navigate(url2);
    EXPECT_EQ(1U, GetPrefs(&profile)->size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(&profile, contents->appid()));
  }
  // Contents are deleted, url should persist.
  EXPECT_EQ(1U, GetPrefs(&profile)->size());
}

TEST_F(BackgroundContentsServiceTest, BackgroundContentsUrlAddedAndClosed) {
  TestingProfile profile;
  BackgroundContentsService service(&profile, command_line_.get());
  BackgroundContentsServiceFactory::GetInstance()->
      RegisterUserPrefsOnBrowserContextForTest(&profile);

  GURL url("http://a/");
  MockBackgroundContents* contents = new MockBackgroundContents(&profile);
  EXPECT_EQ(0U, GetPrefs(&profile)->size());
  contents->SendOpenedNotification(&service);
  contents->Navigate(url);
  EXPECT_EQ(1U, GetPrefs(&profile)->size());
  EXPECT_EQ(url.spec(), GetPrefURLForApp(&profile, contents->appid()));

  // Fake a window closed by script.
  contents->MockClose(&profile);
  EXPECT_EQ(0U, GetPrefs(&profile)->size());
}

// Test what happens if a BackgroundContents shuts down (say, due to a renderer
// crash) then is restarted. Should not persist URL twice.
TEST_F(BackgroundContentsServiceTest, RestartBackgroundContents) {
  TestingProfile profile;
  BackgroundContentsService service(&profile, command_line_.get());
  BackgroundContentsServiceFactory::GetInstance()->
      RegisterUserPrefsOnBrowserContextForTest(&profile);

  GURL url("http://a/");
  {
    std::unique_ptr<MockBackgroundContents> contents(
        new MockBackgroundContents(&profile, "appid"));
    contents->SendOpenedNotification(&service);
    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(&profile)->size());
    EXPECT_EQ(url.spec(), GetPrefURLForApp(&profile, contents->appid()));
  }
  // Contents deleted, url should be persisted.
  EXPECT_EQ(1U, GetPrefs(&profile)->size());

  {
    // Reopen the BackgroundContents to the same URL, we should not register the
    // URL again.
    std::unique_ptr<MockBackgroundContents> contents(
        new MockBackgroundContents(&profile, "appid"));
    contents->SendOpenedNotification(&service);
    contents->Navigate(url);
    EXPECT_EQ(1U, GetPrefs(&profile)->size());
  }
}

// Ensures that BackgroundContentsService properly tracks the association
// between a BackgroundContents and its parent extension, including
// unregistering the BC when the extension is uninstalled.
TEST_F(BackgroundContentsServiceTest, TestApplicationIDLinkage) {
  TestingProfile profile;
  BackgroundContentsService service(&profile, command_line_.get());
  BackgroundContentsServiceFactory::GetInstance()->
      RegisterUserPrefsOnBrowserContextForTest(&profile);

  EXPECT_EQ(NULL, service.GetAppBackgroundContents("appid"));
  MockBackgroundContents* contents = new MockBackgroundContents(&profile,
                                                                "appid");
  std::unique_ptr<MockBackgroundContents> contents2(
      new MockBackgroundContents(&profile, "appid2"));
  contents->SendOpenedNotification(&service);
  EXPECT_EQ(contents, service.GetAppBackgroundContents(contents->appid()));
  contents2->SendOpenedNotification(&service);
  EXPECT_EQ(contents2.get(), service.GetAppBackgroundContents(
      contents2->appid()));
  EXPECT_EQ(0U, GetPrefs(&profile)->size());

  // Navigate the contents, then make sure the one associated with the extension
  // is unregistered.
  GURL url("http://a/");
  GURL url2("http://b/");
  contents->Navigate(url);
  EXPECT_EQ(1U, GetPrefs(&profile)->size());
  contents2->Navigate(url2);
  EXPECT_EQ(2U, GetPrefs(&profile)->size());
  service.ShutdownAssociatedBackgroundContents("appid");
  EXPECT_FALSE(service.IsTracked(contents));
  EXPECT_EQ(NULL, service.GetAppBackgroundContents("appid"));
  EXPECT_EQ(1U, GetPrefs(&profile)->size());
  EXPECT_EQ(url2.spec(), GetPrefURLForApp(&profile, contents2->appid()));
}

TEST_F(BackgroundContentsServiceNotificationTest, TestShowBalloon) {
  scoped_refptr<extensions::Extension> extension =
      extension_test_util::LoadManifest("image_loading_tracker", "app.json");
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->GetManifestData("icons"));

  const message_center::Notification* notification =
      CreateCrashNotification(extension);
  EXPECT_FALSE(notification->icon().IsEmpty());
}

TEST_F(BackgroundContentsServiceNotificationTest, TestShowBalloonShutdown) {
  scoped_refptr<extensions::Extension> extension =
      extension_test_util::LoadManifest("image_loading_tracker", "app.json");
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->GetManifestData("icons"));

  std::string notification_id = BackgroundContentsService::
      GetNotificationDelegateIdForExtensionForTesting(extension->id());

  static_cast<TestingBrowserProcess*>(g_browser_process)->SetShuttingDown(true);
  BackgroundContentsService::ShowBalloonForTesting(extension.get(), profile());
  base::RunLoop().RunUntilIdle();
  static_cast<TestingBrowserProcess*>(g_browser_process)
      ->SetShuttingDown(false);

  const message_center::Notification* notification =
      g_browser_process->notification_ui_manager()->FindById(notification_id,
                                                             profile());

  ASSERT_EQ(nullptr, notification);
}

// Verify if a test notification can show the default extension icon for
// a crash notification for an extension without icon.
TEST_F(BackgroundContentsServiceNotificationTest, TestShowBalloonNoIcon) {
  // Extension manifest file with no 'icon' field.
  scoped_refptr<extensions::Extension> extension =
      extension_test_util::LoadManifest("app", "manifest.json");
  ASSERT_TRUE(extension.get());
  ASSERT_FALSE(extension->GetManifestData("icons"));

  const message_center::Notification* notification =
      CreateCrashNotification(extension);
  EXPECT_FALSE(notification->icon().IsEmpty());
}

TEST_F(BackgroundContentsServiceNotificationTest, TestShowTwoBalloons) {
  TestingProfile profile;
  scoped_refptr<extensions::Extension> extension =
      extension_test_util::LoadManifest("app", "manifest.json");
  ASSERT_TRUE(extension.get());
  CreateCrashNotification(extension);
  CreateCrashNotification(extension);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center::NotificationList::Notifications notifications =
      message_center->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
}
