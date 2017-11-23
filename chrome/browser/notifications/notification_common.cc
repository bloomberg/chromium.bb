// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_common.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "content/public/browser/browser_context.h"
#include "ui/message_center/notifier_id.h"

NotificationCommon::Metadata::~Metadata() = default;

PersistentNotificationMetadata::PersistentNotificationMetadata() {
  type = NotificationHandler::Type::WEB_PERSISTENT;
}

PersistentNotificationMetadata::~PersistentNotificationMetadata() = default;

// static
const PersistentNotificationMetadata* PersistentNotificationMetadata::From(
    const Metadata* metadata) {
  if (!metadata || metadata->type != NotificationHandler::Type::WEB_PERSISTENT)
    return nullptr;

  return static_cast<const PersistentNotificationMetadata*>(metadata);
}

// static
void NotificationCommon::OpenNotificationSettings(
    content::BrowserContext* browser_context) {
#if defined(OS_ANDROID)
  // Android settings are opened directly from Java
  NOTIMPLEMENTED();
#elif defined(OS_CHROMEOS)
  chrome::ShowContentSettingsExceptionsForProfile(
      Profile::FromBrowserContext(browser_context),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
#else
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      Profile::FromBrowserContext(browser_context));
  chrome::ShowContentSettingsExceptions(browser_displayer.browser(),
                                        CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
#endif
}
