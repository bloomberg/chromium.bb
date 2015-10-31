// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_webusb_browser_client.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/common/origin_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "url/gurl.h"

namespace {

const char kWebUsbDetectorNotificationID[] = "webusb.detector";

Browser* GetBrowser() {
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      ProfileManager::GetActiveUserProfile(), chrome::GetActiveDesktop());
  DCHECK(browser_displayer.browser());
  return browser_displayer.browser();
}

void OpenURL(const GURL& url) {
  GetBrowser()->OpenURL(
      content::OpenURLParams(url, content::Referrer(), NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL, true));
}

// Delegate for webusb notification
class WebUsbNotificationDelegate : public message_center::NotificationDelegate {
 public:
  WebUsbNotificationDelegate(const GURL& landing_page,
                             const std::string& notification_id)
      : landing_page_(landing_page), notification_id_(notification_id) {}

  void Click() override {
    OpenURL(landing_page_);
    message_center::MessageCenter::Get()->RemoveNotification(
        notification_id_, false /* by_user */);
  }

 private:
  ~WebUsbNotificationDelegate() override = default;

  GURL landing_page_;
  std::string notification_id_;

  DISALLOW_COPY_AND_ASSIGN(WebUsbNotificationDelegate);
};

}  // namespace

ChromeWebUsbBrowserClient::ChromeWebUsbBrowserClient() {}

ChromeWebUsbBrowserClient::~ChromeWebUsbBrowserClient() {}

void ChromeWebUsbBrowserClient::OnDeviceAdded(
    const base::string16& product_name,
    const GURL& landing_page,
    const std::string& notification_id) {
  if (!content::IsOriginSecure(landing_page)) {
    return;
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  message_center::RichNotificationData rich_notification_data;
  scoped_ptr<message_center::Notification> notification(
      new message_center::Notification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          l10n_util::GetStringFUTF16(
              IDS_WEBUSB_DEVICE_DETECTED_NOTIFICATION_TITLE, product_name),
          l10n_util::GetStringFUTF16(
              IDS_WEBUSB_DEVICE_DETECTED_NOTIFICATION,
              base::UTF8ToUTF16(landing_page.GetContent())),
          rb.GetNativeImageNamed(IDR_USB_NOTIFICATION_ICON),
          base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              kWebUsbDetectorNotificationID),
          rich_notification_data,
          new WebUsbNotificationDelegate(landing_page, notification_id)));

  notification->SetSystemPriority();
  message_center::MessageCenter::Get()->AddNotification(notification.Pass());
}

void ChromeWebUsbBrowserClient::OnDeviceRemoved(
    const std::string& notification_id) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (message_center->FindVisibleNotificationById(notification_id)) {
    message_center->RemoveNotification(notification_id, false /* by_user */);
  }
}
