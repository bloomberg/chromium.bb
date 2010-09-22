// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"

#include <stack>
#include <vector>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

// TODO(sanjeevr): Localize the product name?
const char kCloudPrintProductName[] = "Google Cloud Print";

class CloudPrintProxyService::TokenExpiredNotificationDelegate
    : public NotificationDelegate {
 public:
  explicit TokenExpiredNotificationDelegate(
      CloudPrintProxyService* cloud_print_service)
          : cloud_print_service_(cloud_print_service) {
  }
  void Display() {}
  void Error() {
    cloud_print_service_->OnTokenExpiredNotificationError();
  }
  void Close(bool by_user) {
    cloud_print_service_->OnTokenExpiredNotificationClosed(by_user);
  }
  void Click() {
    cloud_print_service_->OnTokenExpiredNotificationClick();
  }
  std::string id() const { return "cloudprint.tokenexpired"; }

 private:
  CloudPrintProxyService* cloud_print_service_;
  DISALLOW_COPY_AND_ASSIGN(TokenExpiredNotificationDelegate);
};

CloudPrintProxyService::CloudPrintProxyService(Profile* profile)
    : profile_(profile), token_expired_delegate_(NULL) {
}

CloudPrintProxyService::~CloudPrintProxyService() {
  Shutdown();
}

void CloudPrintProxyService::Initialize() {
}


void CloudPrintProxyService::EnableForUser(const std::string& auth_token) {
  // TODO(sanjeevr): Add code to communicate with the cloud print proxy code
  // running in the service process here.
}

void CloudPrintProxyService::DisableForUser() {
  Shutdown();
}

void CloudPrintProxyService::Shutdown() {
  // TODO(sanjeevr): Add code to communicate with the cloud print proxy code
  // running in the service process here.
}

bool CloudPrintProxyService::ShowTokenExpiredNotification() {
  // If we already have a pending notification, don't show another one.
  if (token_expired_delegate_.get())
    return false;

  // TODO(sanjeevr): Get icon for this notification.
  GURL icon_url;

  string16 title = UTF8ToUTF16(kCloudPrintProductName);
  string16 message =
      l10n_util::GetStringUTF16(IDS_CLOUD_PRINT_TOKEN_EXPIRED_MESSAGE);
  string16 content_url = DesktopNotificationService::CreateDataUrl(
      icon_url, title, message, WebKit::WebTextDirectionDefault);
  token_expired_delegate_ = new TokenExpiredNotificationDelegate(this);
  Notification notification(GURL(), GURL(content_url), string16(), string16(),
                            token_expired_delegate_.get());
  g_browser_process->notification_ui_manager()->Add(notification, profile_);
  // Keep the browser alive while we are showing the notification.
  BrowserList::StartKeepAlive();
  return true;
}

void CloudPrintProxyService::OnTokenExpiredNotificationError() {
  TokenExpiredNotificationDone();
}

void CloudPrintProxyService::OnTokenExpiredNotificationClosed(bool by_user) {
  TokenExpiredNotificationDone();
}

void CloudPrintProxyService::OnTokenExpiredNotificationClick() {
  TokenExpiredNotificationDone();
  // TODO(sanjeevr): Launch the cloud print setup flow.
}

void CloudPrintProxyService::TokenExpiredNotificationDone() {
  if (token_expired_delegate_.get()) {
    g_browser_process->notification_ui_manager()->Cancel(
        Notification(GURL(), GURL(), string16(), string16(),
                     token_expired_delegate_.get()));
    token_expired_delegate_ = NULL;
    BrowserList::EndKeepAlive();
  }
}

