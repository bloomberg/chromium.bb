// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/tether_notification_presenter.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace {
const int kTestNetworkSignalStrength = 50;
}  // namespace

namespace chromeos {

namespace tether {

namespace {

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() : message_center::FakeMessageCenter() {}
  ~TestMessageCenter() override {}

  void NotifyNotificationTapped(const std::string& notification_id) {
    for (auto& observer : observer_list_) {
      observer.OnNotificationClicked(notification_id);
    }
  }

  void NotifyNotificationButtonTapped(const std::string& notification_id,
                                      int button_index) {
    for (auto& observer : observer_list_) {
      observer.OnNotificationButtonClicked(notification_id, button_index);
    }
  }

  size_t GetNumNotifications() { return notifications_.size(); }

  // message_center::FakeMessageCenter:
  void AddObserver(message_center::MessageCenterObserver* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(
      message_center::MessageCenterObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override {
    auto iter = std::find_if(
        notifications_.begin(), notifications_.end(),
        [id](const std::shared_ptr<message_center::Notification> notification) {
          return notification->id() == id;
        });
    return iter != notifications_.end() ? iter->get() : nullptr;
  }

  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    notifications_.push_back(std::move(notification));
  }

  void UpdateNotification(
      const std::string& old_id,
      std::unique_ptr<message_center::Notification> new_notification) override {
    RemoveNotification(old_id, false /* by_user */);
    AddNotification(std::move(new_notification));
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    notifications_.erase(std::find_if(
        notifications_.begin(), notifications_.end(),
        [id](const std::shared_ptr<message_center::Notification> notification) {
          return notification->id() == id;
        }));
  }

 private:
  std::vector<std::shared_ptr<message_center::Notification>> notifications_;
  base::ObserverList<message_center::MessageCenterObserver> observer_list_;
};

cryptauth::RemoteDevice CreateTestRemoteDevice() {
  cryptauth::RemoteDevice device = cryptauth::GenerateTestRemoteDevices(1)[0];
  device.name = "testDevice";
  return device;
}

}  // namespace

class TetherNotificationPresenterTest : public testing::Test {
 public:
  class TestNetworkConnect : public NetworkConnect {
   public:
    TestNetworkConnect() {}
    ~TestNetworkConnect() override {}

    std::string network_id_to_connect() { return network_id_to_connect_; }

    // NetworkConnect:
    void DisconnectFromNetworkId(const std::string& network_id) override {}
    bool MaybeShowConfigureUI(const std::string& network_id,
                              const std::string& connect_error) override {
      return false;
    }
    void SetTechnologyEnabled(const chromeos::NetworkTypePattern& technology,
                              bool enabled_state) override {}
    void ShowMobileSetup(const std::string& network_id) override {}
    void ConfigureNetworkIdAndConnect(
        const std::string& network_id,
        const base::DictionaryValue& shill_properties,
        bool shared) override {}
    void CreateConfigurationAndConnect(base::DictionaryValue* shill_properties,
                                       bool shared) override {}
    void CreateConfiguration(base::DictionaryValue* shill_properties,
                             bool shared) override {}

    void ConnectToNetworkId(const std::string& network_id) override {
      network_id_to_connect_ = network_id;
    }

   private:
    std::string network_id_to_connect_;
  };

  class TestSettingsUiDelegate
      : public TetherNotificationPresenter::SettingsUiDelegate {
   public:
    TestSettingsUiDelegate() {}
    ~TestSettingsUiDelegate() override {}

    Profile* last_profile() { return last_profile_; }
    std::string last_settings_subpage() { return last_settings_subpage_; }

    // TetherNotificationPresenter::SettingsUiDelegate:
    void ShowSettingsSubPageForProfile(Profile* profile,
                                       const std::string& sub_page) override {
      last_profile_ = profile;
      last_settings_subpage_ = sub_page;
    }

   private:
    Profile* last_profile_ = nullptr;
    std::string last_settings_subpage_;
  };

 protected:
  TetherNotificationPresenterTest() : test_device_(CreateTestRemoteDevice()) {}

  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
    test_message_center_ = base::WrapUnique(new TestMessageCenter());
    test_network_connect_ = base::WrapUnique(new TestNetworkConnect());
    notification_presenter_ = base::WrapUnique(new TetherNotificationPresenter(
        profile_.get(), test_message_center_.get(),
        test_network_connect_.get()));

    test_settings_ui_delegate_ = new TestSettingsUiDelegate();
    notification_presenter_->SetSettingsUiDelegateForTesting(
        base::WrapUnique(test_settings_ui_delegate_));
  }

  void TearDown() override { profile_.reset(); }

  std::string GetActiveHostNotificationId() {
    return std::string(TetherNotificationPresenter::kActiveHostNotificationId);
  }

  std::string GetPotentialHotspotNotificationId() {
    return std::string(
        TetherNotificationPresenter::kPotentialHotspotNotificationId);
  }

  std::string GetSetupRequiredNotificationId() {
    return std::string(
        TetherNotificationPresenter::kSetupRequiredNotificationId);
  }

  void VerifySettingsOpened() {
    EXPECT_EQ(profile_.get(), test_settings_ui_delegate_->last_profile());
    EXPECT_EQ("networks?type=Tether",
              test_settings_ui_delegate_->last_settings_subpage());
  }

  void VerifySettingsNotOpened() {
    EXPECT_FALSE(test_settings_ui_delegate_->last_profile());
    EXPECT_TRUE(test_settings_ui_delegate_->last_settings_subpage().empty());
  }

  const content::TestBrowserThreadBundle thread_bundle_;
  const cryptauth::RemoteDevice test_device_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestMessageCenter> test_message_center_;
  std::unique_ptr<TestNetworkConnect> test_network_connect_;
  TestSettingsUiDelegate* test_settings_ui_delegate_;
  std::unique_ptr<TetherNotificationPresenter> notification_presenter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherNotificationPresenterTest);
};

TEST_F(TetherNotificationPresenterTest,
       TestHostConnectionFailedNotification_RemoveProgrammatically) {
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetActiveHostNotificationId()));
  notification_presenter_->NotifyConnectionToHostFailed();

  message_center::Notification* notification =
      test_message_center_->FindVisibleNotificationById(
          GetActiveHostNotificationId());
  EXPECT_TRUE(notification);
  EXPECT_EQ(GetActiveHostNotificationId(), notification->id());

  EXPECT_EQ(1u, test_message_center_->GetNumNotifications());
  notification_presenter_->RemoveConnectionToHostFailedNotification();
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetActiveHostNotificationId()));
  EXPECT_EQ(0u, test_message_center_->GetNumNotifications());

  VerifySettingsNotOpened();
}

TEST_F(TetherNotificationPresenterTest,
       TestHostConnectionFailedNotification_TapNotification) {
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetActiveHostNotificationId()));
  notification_presenter_->NotifyConnectionToHostFailed();

  message_center::Notification* notification =
      test_message_center_->FindVisibleNotificationById(
          GetActiveHostNotificationId());
  EXPECT_TRUE(notification);
  EXPECT_EQ(GetActiveHostNotificationId(), notification->id());

  // Tap the notification.
  test_message_center_->NotifyNotificationTapped(GetActiveHostNotificationId());
  VerifySettingsOpened();
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetActiveHostNotificationId()));
  EXPECT_EQ(0u, test_message_center_->GetNumNotifications());
}

TEST_F(TetherNotificationPresenterTest, TestSetupRequiredNotification) {
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetSetupRequiredNotificationId()));
  notification_presenter_->NotifySetupRequired(test_device_.name);

  message_center::Notification* notification =
      test_message_center_->FindVisibleNotificationById(
          GetSetupRequiredNotificationId());
  EXPECT_TRUE(notification);
  EXPECT_EQ(GetSetupRequiredNotificationId(), notification->id());

  EXPECT_EQ(1u, test_message_center_->GetNumNotifications());
  notification_presenter_->RemoveSetupRequiredNotification();
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetSetupRequiredNotificationId()));
  EXPECT_EQ(0u, test_message_center_->GetNumNotifications());
}

TEST_F(TetherNotificationPresenterTest,
       TestSinglePotentialHotspotNotification_RemoveProgrammatically) {
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyPotentialHotspotNearby(
      test_device_, kTestNetworkSignalStrength);

  message_center::Notification* notification =
      test_message_center_->FindVisibleNotificationById(
          GetPotentialHotspotNotificationId());
  EXPECT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  notification_presenter_->RemovePotentialHotspotNotification();
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));

  VerifySettingsNotOpened();
}

TEST_F(TetherNotificationPresenterTest,
       TestSinglePotentialHotspotNotification_TapNotification) {
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyPotentialHotspotNearby(
      test_device_, kTestNetworkSignalStrength);

  message_center::Notification* notification =
      test_message_center_->FindVisibleNotificationById(
          GetPotentialHotspotNotificationId());
  EXPECT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  // Tap the notification.
  test_message_center_->NotifyNotificationTapped(
      GetPotentialHotspotNotificationId());
  VerifySettingsOpened();
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));
  EXPECT_EQ(0u, test_message_center_->GetNumNotifications());
}

TEST_F(TetherNotificationPresenterTest,
       TestSinglePotentialHotspotNotification_TapNotificationButton) {
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyPotentialHotspotNearby(
      test_device_, kTestNetworkSignalStrength);

  message_center::Notification* notification =
      test_message_center_->FindVisibleNotificationById(
          GetPotentialHotspotNotificationId());
  EXPECT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  // Tap the notification's button.
  test_message_center_->NotifyNotificationButtonTapped(
      GetPotentialHotspotNotificationId(), 0 /* button_index */);
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));
  EXPECT_EQ(0u, test_message_center_->GetNumNotifications());

  EXPECT_EQ(test_device_.GetDeviceId(),
            test_network_connect_->network_id_to_connect());
}

TEST_F(TetherNotificationPresenterTest,
       TestMultiplePotentialHotspotNotification_RemoveProgrammatically) {
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyMultiplePotentialHotspotsNearby();

  message_center::Notification* notification =
      test_message_center_->FindVisibleNotificationById(
          GetPotentialHotspotNotificationId());
  EXPECT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  notification_presenter_->RemovePotentialHotspotNotification();
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));

  VerifySettingsNotOpened();
}

TEST_F(TetherNotificationPresenterTest,
       TestMultiplePotentialHotspotNotification_TapNotification) {
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyMultiplePotentialHotspotsNearby();

  message_center::Notification* notification =
      test_message_center_->FindVisibleNotificationById(
          GetPotentialHotspotNotificationId());
  EXPECT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  // Tap the notification.
  test_message_center_->NotifyNotificationTapped(
      GetPotentialHotspotNotificationId());
  VerifySettingsOpened();
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));
  EXPECT_EQ(0u, test_message_center_->GetNumNotifications());
}

TEST_F(TetherNotificationPresenterTest,
       TestPotentialHotspotNotifications_UpdatesOneNotification) {
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyPotentialHotspotNearby(
      test_device_, kTestNetworkSignalStrength);

  message_center::Notification* notification =
      test_message_center_->FindVisibleNotificationById(
          GetPotentialHotspotNotificationId());
  EXPECT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());
  EXPECT_EQ(1u, test_message_center_->GetNumNotifications());

  // Simulate more device results coming in. Display the potential hotspots
  // notification for multiple devices.
  notification_presenter_->NotifyMultiplePotentialHotspotsNearby();

  // The existing notification should have been updated instead of creating a
  // new one.
  notification = test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId());
  EXPECT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());
  EXPECT_EQ(1u, test_message_center_->GetNumNotifications());

  notification_presenter_->RemovePotentialHotspotNotification();
  EXPECT_FALSE(test_message_center_->FindVisibleNotificationById(
      GetPotentialHotspotNotificationId()));

  VerifySettingsNotOpened();
}

TEST_F(TetherNotificationPresenterTest,
       TestDoesNotOpenSettingsWhenOtherNotificationClicked) {
  test_message_center_->NotifyNotificationTapped("otherNotificationId");
  VerifySettingsNotOpened();
}

}  // namespace tether

}  // namespace chromeos
