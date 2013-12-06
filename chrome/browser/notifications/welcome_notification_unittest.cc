// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/welcome_notification.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "sync/api/sync_error_factory_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/notification.h"

const char kChromeNowExtensionID[] = "pafkbggdmjlpgkdkcbjmhmfcdpncadgh";

class MockMessageCenter : public message_center::FakeMessageCenter {
 public:
  MockMessageCenter()
    : add_notification_calls_(0),
      remove_notification_calls_(0),
      notifications_with_shown_as_popup_(0) {};

  int add_notification_calls() { return add_notification_calls_; }
  int remove_notification_calls() { return remove_notification_calls_; }
  int notifications_with_shown_as_popup() {
    return notifications_with_shown_as_popup_;
  }

  // message_center::FakeMessageCenter Overrides
  virtual bool HasNotification(const std::string& id) OVERRIDE {
    return last_notification.get() &&
        (last_notification->id() == id);
  }

  virtual void AddNotification(
      scoped_ptr<message_center::Notification> notification) OVERRIDE {
    EXPECT_FALSE(last_notification.get());
    last_notification.swap(notification);
    add_notification_calls_++;
    if (last_notification->shown_as_popup())
      notifications_with_shown_as_popup_++;
  }

  virtual void RemoveNotification(const std::string& id, bool by_user)
      OVERRIDE {
    EXPECT_TRUE(last_notification.get());
    last_notification.reset();
    remove_notification_calls_++;
  }

  void CloseCurrentNotification() {
    EXPECT_TRUE(last_notification.get());
    last_notification->delegate()->Close(true);
    RemoveNotification(last_notification->id(), true);
  }

 private:
  scoped_ptr<message_center::Notification> last_notification;
  int add_notification_calls_;
  int remove_notification_calls_;
  int notifications_with_shown_as_popup_;
};

class TestSyncProcessor : public syncer::SyncChangeProcessor {
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE {
    return syncer::SyncError();
  }

  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE {
    return syncer::SyncDataList();
  }
};

class WelcomeNotificationTest : public testing::Test {
 protected:
  WelcomeNotificationTest() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry(
        new user_prefs::PrefRegistrySyncable());
    WelcomeNotification::RegisterProfilePrefs(pref_registry.get());
  }

  virtual void SetUp() {
    message_loop_.reset(new base::MessageLoop());
    profile_.reset(new TestingProfile());
    message_center_.reset(new MockMessageCenter());
    welcome_notification_.reset(
        new WelcomeNotification(profile_.get(), message_center_.get()));
  }

  virtual void TearDown() {
    welcome_notification_.reset();
    message_center_.reset();
    profile_.reset();
    message_loop_.reset();
  }

  void StartPreferenceSyncing() {
    PrefServiceSyncable::FromProfile(profile())->GetSyncableService(
        syncer::PREFERENCES)->MergeDataAndStartSyncing(
            syncer::PREFERENCES,
            syncer::SyncDataList(),
            scoped_ptr<syncer::SyncChangeProcessor>(new TestSyncProcessor),
            scoped_ptr<syncer::SyncErrorFactory>(
                new syncer::SyncErrorFactoryMock()));
  }

  void ShowChromeNowNotification() {
    ShowNotification(
        "ChromeNowNotification",
        message_center::NotifierId(
            message_center::NotifierId::APPLICATION,
            kChromeNowExtensionID));
  }

  void ShowRegularNotification() {
    ShowNotification(
        "RegularNotification",
        message_center::NotifierId(
            message_center::NotifierId::APPLICATION,
            "aaaabbbbccccddddeeeeffffggghhhhi"));
  }

  void FlushMessageLoop() {
    message_loop_->RunUntilIdle();
  }

  TestingProfile* profile() { return profile_.get(); }
  MockMessageCenter* message_center() { return message_center_.get(); }

 private:
  class TestNotificationDelegate : public NotificationDelegate {
   public:
    explicit TestNotificationDelegate(const std::string& id)
        : id_(id) {}

    // Overridden from NotificationDelegate:
    virtual void Display() OVERRIDE {}
    virtual void Error() OVERRIDE {}
    virtual void Close(bool by_user) OVERRIDE {}
    virtual void Click() OVERRIDE {}
    virtual void ButtonClick(int index) OVERRIDE {}

    virtual std::string id() const OVERRIDE { return id_; }

    virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE {
      return NULL;
    }

   private:
    virtual ~TestNotificationDelegate() {}

    const std::string id_;

    DISALLOW_COPY_AND_ASSIGN(TestNotificationDelegate);
  };

  void ShowNotification(
      std::string notification_id,
      const message_center::NotifierId& notifier_id) {
    message_center::RichNotificationData rich_notification_data;
    rich_notification_data.priority = 0;
    Notification notification(
        message_center::NOTIFICATION_TYPE_BASE_FORMAT,
        GURL("http://tests.url"),
        base::UTF8ToUTF16("Title"),
        base::UTF8ToUTF16("Body"),
        gfx::Image(),
        blink::WebTextDirectionDefault,
        notifier_id,
        base::UTF8ToUTF16("Source"),
        base::UTF8ToUTF16(notification_id),
        rich_notification_data,
        new TestNotificationDelegate("TestNotification"));
    welcome_notification_->ShowWelcomeNotificationIfNecessary(notification);
  }

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<MockMessageCenter> message_center_;
  scoped_ptr<WelcomeNotification> welcome_notification_;
  scoped_ptr<base::MessageLoop> message_loop_;
};

// Show a regular notification. Expect that WelcomeNotification will
// not show a welcome notification.
TEST_F(WelcomeNotificationTest, FirstRunShowRegularNotification) {
  StartPreferenceSyncing();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  ShowRegularNotification();

  EXPECT_TRUE(message_center()->add_notification_calls() == 0);
  EXPECT_TRUE(message_center()->remove_notification_calls() == 0);
  EXPECT_TRUE(message_center()->notifications_with_shown_as_popup() == 0);
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));
}

// Show a Chrome Now notification. Expect that WelcomeNotification will
// show a welcome notification.
TEST_F(WelcomeNotificationTest, FirstRunChromeNowNotification) {
  StartPreferenceSyncing();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  ShowChromeNowNotification();

  EXPECT_TRUE(message_center()->add_notification_calls() == 1);
  EXPECT_TRUE(message_center()->remove_notification_calls() == 0);
  EXPECT_TRUE(message_center()->notifications_with_shown_as_popup() == 0);
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));
}

// Show a Chrome Now notification that was already shown before.
TEST_F(WelcomeNotificationTest, ShowWelcomeNotificationAgain) {
  StartPreferenceSyncing();
  profile()->GetPrefs()->SetBoolean(
      prefs::kWelcomeNotificationPreviouslyPoppedUp, true);
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  ShowChromeNowNotification();

  EXPECT_TRUE(message_center()->add_notification_calls() == 1);
  EXPECT_TRUE(message_center()->remove_notification_calls() == 0);
  EXPECT_TRUE(message_center()->notifications_with_shown_as_popup() == 1);
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));
}

// Don't show a welcome notification if it was previously dismissed
TEST_F(WelcomeNotificationTest, WelcomeNotificationPreviouslyDismissed) {
  StartPreferenceSyncing();
  profile()->GetPrefs()->SetBoolean(prefs::kWelcomeNotificationDismissed, true);
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  ShowChromeNowNotification();

  EXPECT_TRUE(message_center()->add_notification_calls() == 0);
  EXPECT_TRUE(message_center()->remove_notification_calls() == 0);
  EXPECT_TRUE(message_center()->notifications_with_shown_as_popup() == 0);
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));
}

// Show a Chrome Now notification and dismiss it.
// Expect welcome toast dismissed to be true.
TEST_F(WelcomeNotificationTest, DismissWelcomeNotification) {
  StartPreferenceSyncing();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  ShowChromeNowNotification();
  message_center()->CloseCurrentNotification();
  FlushMessageLoop();

  EXPECT_TRUE(message_center()->add_notification_calls() == 1);
  EXPECT_TRUE(message_center()->remove_notification_calls() == 1);
  EXPECT_TRUE(message_center()->notifications_with_shown_as_popup() == 0);
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));
}

// Show a Chrome Now notification and dismiss it via a synced preference change.
// Expect welcome toast dismissed to be true.
TEST_F(WelcomeNotificationTest, SyncedDismissalWelcomeNotification) {
  StartPreferenceSyncing();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  ShowChromeNowNotification();
  profile()->GetPrefs()->SetBoolean(prefs::kWelcomeNotificationDismissed, true);

  EXPECT_TRUE(message_center()->add_notification_calls() == 1);
  EXPECT_TRUE(message_center()->remove_notification_calls() == 1);
  EXPECT_TRUE(message_center()->notifications_with_shown_as_popup() == 0);
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));
}

// Simulate a delayed preference sync when the welcome notification was
// previously dismissed.
TEST_F(WelcomeNotificationTest, DelayedPreferenceSyncPreviouslyDismissed) {
  // Show a notification while the preference system is not syncing.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  ShowChromeNowNotification();

  EXPECT_TRUE(message_center()->add_notification_calls() == 0);
  EXPECT_TRUE(message_center()->remove_notification_calls() == 0);
  EXPECT_TRUE(message_center()->notifications_with_shown_as_popup() == 0);
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  // Now start the preference syncing with a previously dismissed welcome.
  profile()->GetPrefs()->SetBoolean(prefs::kWelcomeNotificationDismissed, true);
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  StartPreferenceSyncing();

  EXPECT_TRUE(message_center()->add_notification_calls() == 0);
  EXPECT_TRUE(message_center()->remove_notification_calls() == 0);
  EXPECT_TRUE(message_center()->notifications_with_shown_as_popup() == 0);
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));
}

// Simulate a delayed preference sync when the welcome notification was
// never shown.
TEST_F(WelcomeNotificationTest, DelayedPreferenceSyncNeverShown) {
  // Show a notification while the preference system is not syncing.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  ShowChromeNowNotification();

  EXPECT_TRUE(message_center()->add_notification_calls() == 0);
  EXPECT_TRUE(message_center()->remove_notification_calls() == 0);
  EXPECT_TRUE(message_center()->notifications_with_shown_as_popup() == 0);
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  // Now start the preference syncing with the default preference values.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));

  StartPreferenceSyncing();

  EXPECT_TRUE(message_center()->add_notification_calls() == 1);
  EXPECT_TRUE(message_center()->remove_notification_calls() == 0);
  EXPECT_TRUE(message_center()->notifications_with_shown_as_popup() == 0);
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(
          prefs::kWelcomeNotificationPreviouslyPoppedUp));
}
