// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_notification.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
const char* kNotifierId = "app.downloaded-notification";
const char* kNotificationId = "EXTENSION_INSTALLED_NOTIFICATION";
}  // anonymous namespace

using content::BrowserThread;

// static
void ExtensionInstalledNotification::Show(
    const extensions::Extension* extension, Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(g_browser_process->notification_ui_manager());

  // It's lifetime is managed by the parent class NotificationDelegate.
  new ExtensionInstalledNotification(extension, profile);
}

ExtensionInstalledNotification::ExtensionInstalledNotification(
    const extensions::Extension* extension, Profile* profile)
    : extension_id_(extension->id()), profile_(profile) {

  message_center::RichNotificationData optional_field;
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      base::UTF8ToUTF16(extension->name()),
      l10n_util::GetStringUTF16(IDS_EXTENSION_NOTIFICATION_INSTALLED),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_EXTENSION_INSTALLED),
      message_center::NotifierId(
          message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId),
      base::string16() /* display_source */,
      GURL(extension_urls::kChromeWebstoreBaseURL) /* origin_url */,
      kNotificationId, optional_field, this));
  g_browser_process->notification_ui_manager()->Add(*notification, profile_);
}

ExtensionInstalledNotification::~ExtensionInstalledNotification() {}

void ExtensionInstalledNotification::Click() {
  if (!extensions::util::IsAppLaunchable(extension_id_, profile_))
    return;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_id_, extensions::ExtensionRegistry::EVERYTHING);
  if (!extension)
    return;

  AppLaunchParams params = CreateAppLaunchParamsUserContainer(
      profile_, extension, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      extensions::SOURCE_INSTALLED_NOTIFICATION);
  OpenApplication(params);
}

std::string ExtensionInstalledNotification::id() const {
  return kNotificationId;
}
