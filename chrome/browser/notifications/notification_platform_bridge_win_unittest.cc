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
#include "base/test/scoped_task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "chrome/browser/notifications/mock_notification_image_retainer.h"
#include "chrome/browser/notifications/notification_template_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notifier_id.h"

namespace mswr = Microsoft::WRL;
namespace winui = ABI::Windows::UI;

using message_center::Notification;

namespace {

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
      ABI::Windows::UI::Notifications::IToastNotification** toast) {
    GURL origin(kOrigin);
    auto notification = std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, L"title",
        L"message", gfx::Image(), L"display_source", origin,
        message_center::NotifierId(origin),
        message_center::RichNotificationData(), nullptr /* delegate */);
    MockNotificationImageRetainer image_retainer;
    std::unique_ptr<NotificationTemplateBuilder> builder =
        NotificationTemplateBuilder::Build(&image_retainer, kProfileId,
                                           *notification);

    return notification_platform_bridge_win_->GetToastNotificationForTesting(
        *notification, *builder, toast);
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<NotificationPlatformBridgeWin>
      notification_platform_bridge_win_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeWinTest);
};

TEST_F(NotificationPlatformBridgeWinTest, GroupAndTag) {
  // This test requires WinRT core functions, which are not available in
  // older versions of Windows.
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  base::win::ScopedCOMInitializer com_initializer;

  mswr::ComPtr<winui::Notifications::IToastNotification> toast;
  ASSERT_HRESULT_SUCCEEDED(GetToast(&toast));
  mswr::ComPtr<winui::Notifications::IToastNotification2> toast2;
  ASSERT_HRESULT_SUCCEEDED(
      toast.As<winui::Notifications::IToastNotification2>(&toast2));

  HSTRING hstring_group;
  ASSERT_HRESULT_SUCCEEDED(toast2->get_Group(&hstring_group));
  base::win::ScopedHString group(hstring_group);
  GURL origin(kOrigin);
  ASSERT_STREQ(base::UintToString16(base::Hash(origin.spec())).c_str(),
               group.Get().as_string().c_str());

  HSTRING hstring_tag;
  ASSERT_HRESULT_SUCCEEDED(toast2->get_Tag(&hstring_tag));
  base::win::ScopedHString tag(hstring_tag);
  ASSERT_STREQ(base::UintToString16(base::Hash(kNotificationId)).c_str(),
               tag.Get().as_string().c_str());
}
