// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_infobar_delegate.h"

#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* DesktopNotificationInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages) {
  const content::NavigationEntry* committed_entry =
      infobar_service->web_contents()->GetController().GetLastCommittedEntry();
  return infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new DesktopNotificationInfoBarDelegate(
          controller, id, requesting_frame,
          committed_entry ? committed_entry->GetUniqueID() : 0,
          display_languages))));
}

DesktopNotificationInfoBarDelegate::DesktopNotificationInfoBarDelegate(
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    int contents_unique_id,
    const std::string& display_languages)
    : PermissionInfobarDelegate(controller, id, requesting_frame,
                                CONTENT_SETTINGS_TYPE_NOTIFICATIONS),
      requesting_frame_(requesting_frame),
      display_languages_(display_languages) {
}

DesktopNotificationInfoBarDelegate::~DesktopNotificationInfoBarDelegate() {
}

int DesktopNotificationInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_DESKTOP_NOTIFICATIONS;
}

base::string16 DesktopNotificationInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_NOTIFICATION_PERMISSIONS,
      net::FormatUrl(requesting_frame_.GetOrigin(), display_languages_,
                     net::kFormatUrlOmitUsernamePassword |
                     net::kFormatUrlOmitTrailingSlashOnBareHostname,
                     net::UnescapeRule::SPACES, NULL, NULL, NULL));
}
