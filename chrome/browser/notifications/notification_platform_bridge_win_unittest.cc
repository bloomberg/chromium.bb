// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_win.h"

#include <windows.ui.notifications.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <memory>

#include "base/hash.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "chrome/browser/notifications/mock_itoastnotification.h"
#include "chrome/browser/notifications/mock_notification_image_retainer.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_template_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notifier_id.h"

namespace mswr = Microsoft::WRL;
namespace winui = ABI::Windows::UI;

using message_center::Notification;

namespace {

const char kEncodedId[] = "0|Default|0|https://example.com/|notification_id";
const char kOrigin[] = "https://www.google.com/";
const char kNotificationId[] = "id";
const char kProfileId[] = "Default";

}  // namespace

class NotificationPlatformBridgeWinTest : public testing::Test {
 public:
  NotificationPlatformBridgeWinTest()
      : notification_platform_bridge_win_(
            std::make_unique<NotificationPlatformBridgeWin>()) {}
  ~NotificationPlatformBridgeWinTest() override = default;

  HRESULT GetToast(
      bool renotify,
      mswr::ComPtr<winui::Notifications::IToastNotification2>* toast2) {
    GURL origin(kOrigin);
    auto notification = std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, L"title",
        L"message", gfx::Image(), L"display_source", origin,
        message_center::NotifierId(origin),
        message_center::RichNotificationData(), nullptr /* delegate */);
    notification->set_renotify(renotify);
    MockNotificationImageRetainer image_retainer;
    std::unique_ptr<NotificationTemplateBuilder> builder =
        NotificationTemplateBuilder::Build(&image_retainer, kEncodedId,
                                           kProfileId, *notification);

    mswr::ComPtr<winui::Notifications::IToastNotification> toast;
    HRESULT hr =
        notification_platform_bridge_win_->GetToastNotificationForTesting(
            *notification, *builder, &toast);
    if (FAILED(hr)) {
      LOG(ERROR) << "GetToastNotificationForTesting failed";
      return hr;
    }

    hr = toast.As<winui::Notifications::IToastNotification2>(toast2);
    if (FAILED(hr)) {
      LOG(ERROR) << "Converting to IToastNotification2 failed";
      return hr;
    }

    return S_OK;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<NotificationPlatformBridgeWin>
      notification_platform_bridge_win_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeWinTest);
};

TEST_F(NotificationPlatformBridgeWinTest, EncodeDecode) {
  NotificationHandler::Type notification_type =
      NotificationHandler::Type::WEB_PERSISTENT;
  std::string notification_id = "Foo";
  std::string profile_id = "Bar";
  bool incognito = false;
  GURL origin_url("http://www.google.com/");

  std::string encoded = notification_platform_bridge_win_->EncodeTemplateId(
      notification_type, notification_id, profile_id, incognito, origin_url);

  NotificationHandler::Type decoded_notification_type;
  std::string decoded_notification_id;
  std::string decoded_profile_id;
  bool decoded_incognito;
  GURL decoded_origin_url;

  // Empty string.
  EXPECT_FALSE(notification_platform_bridge_win_->DecodeTemplateId(
      "", &decoded_notification_type, &decoded_notification_id,
      &decoded_profile_id, &decoded_incognito, &decoded_origin_url));

  // Actual data.
  EXPECT_TRUE(notification_platform_bridge_win_->DecodeTemplateId(
      encoded, &decoded_notification_type, &decoded_notification_id,
      &decoded_profile_id, &decoded_incognito, &decoded_origin_url));

  EXPECT_EQ(decoded_notification_type, notification_type);
  EXPECT_EQ(decoded_notification_id, notification_id);
  EXPECT_EQ(decoded_profile_id, profile_id);
  EXPECT_EQ(decoded_incognito, incognito);
  EXPECT_EQ(decoded_origin_url, origin_url);

  // Actual data, but only notification_id is requested.
  EXPECT_TRUE(notification_platform_bridge_win_->DecodeTemplateId(
      encoded, nullptr /* notification_type */, &decoded_notification_id,
      nullptr /* profile_id */, nullptr /* incognito */,
      nullptr /* origin_url */));
  EXPECT_EQ(decoded_notification_id, notification_id);

  // Throw in a few extra separators (becomes part of the notification id).
  std::string extra = "|Extra|Data|";
  encoded += extra;
  // Update the expected output as well.
  notification_id += extra;

  EXPECT_TRUE(notification_platform_bridge_win_->DecodeTemplateId(
      encoded, &decoded_notification_type, &decoded_notification_id,
      &decoded_profile_id, &decoded_incognito, &decoded_origin_url));

  EXPECT_EQ(decoded_notification_type, notification_type);
  EXPECT_EQ(decoded_notification_id, notification_id);
  EXPECT_EQ(decoded_profile_id, profile_id);
  EXPECT_EQ(decoded_incognito, incognito);
  EXPECT_EQ(decoded_origin_url, origin_url);
}

TEST_F(NotificationPlatformBridgeWinTest, GroupAndTag) {
  // This test requires WinRT core functions, which are not available in
  // older versions of Windows.
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  base::win::ScopedCOMInitializer com_initializer;

  mswr::ComPtr<winui::Notifications::IToastNotification2> toast2;
  ASSERT_HRESULT_SUCCEEDED(GetToast(false /* renotify */, &toast2));

  HSTRING hstring_group;
  ASSERT_HRESULT_SUCCEEDED(toast2->get_Group(&hstring_group));
  base::win::ScopedHString group(hstring_group);
  // NOTE: If you find yourself needing to change this value, make sure that
  // NotificationPlatformBridgeWinImpl::Close supports specifying the right
  // group value for RemoveGroupedTagWithId.
  ASSERT_STREQ(L"Notifications", group.Get().as_string().c_str());

  HSTRING hstring_tag;
  ASSERT_HRESULT_SUCCEEDED(toast2->get_Tag(&hstring_tag));
  base::win::ScopedHString tag(hstring_tag);
  ASSERT_STREQ(base::UintToString16(base::Hash(kNotificationId)).c_str(),
               tag.Get().as_string().c_str());
}

TEST_F(NotificationPlatformBridgeWinTest, Suppress) {
  // This test requires WinRT core functions, which are not available in
  // older versions of Windows.
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  base::win::ScopedCOMInitializer com_initializer;

  std::vector<winui::Notifications::IToastNotification*> notifications;
  notification_platform_bridge_win_->SetDisplayedNotificationsForTesting(
      &notifications);

  mswr::ComPtr<winui::Notifications::IToastNotification2> toast2;
  boolean suppress;

  // Make sure this works a toast is not suppressed when no notifications are
  // registered.
  ASSERT_HRESULT_SUCCEEDED(GetToast(false, &toast2));
  ASSERT_HRESULT_SUCCEEDED(toast2->get_SuppressPopup(&suppress));
  ASSERT_FALSE(suppress);

  // Register a single notification.
  base::string16 tag = base::UintToString16(base::Hash(kNotificationId));
  MockIToastNotification item1(
      L"<toast launch=\"0|Default|0|https://foo.com/|id\"></toast>", tag);
  notifications.push_back(&item1);

  // Request this notification with renotify true (should not be suppressed).
  ASSERT_HRESULT_SUCCEEDED(GetToast(true, &toast2));
  ASSERT_HRESULT_SUCCEEDED(toast2->get_SuppressPopup(&suppress));
  ASSERT_FALSE(suppress);

  // Request this notification with renotify false (should be suppressed).
  ASSERT_HRESULT_SUCCEEDED(GetToast(false, &toast2));
  ASSERT_HRESULT_SUCCEEDED(toast2->get_SuppressPopup(&suppress));
  ASSERT_TRUE(suppress);

  notification_platform_bridge_win_->SetDisplayedNotificationsForTesting(
      nullptr);
}
