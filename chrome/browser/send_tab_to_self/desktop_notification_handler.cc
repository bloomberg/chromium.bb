// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/strings/grit/ui_strings.h"

namespace send_tab_to_self {

DesktopNotificationHandler::DesktopNotificationHandler(Profile* profile)
    : profile_(profile) {}

DesktopNotificationHandler::~DesktopNotificationHandler() = default;

void DesktopNotificationHandler::DisplayNewEntry(
    const SendTabToSelfEntry* entry) {
  const base::string16& device_info =
      l10n_util::GetStringUTF16(
          IDS_MESSAGE_NOTIFICATION_SEND_TAB_TO_SELF_DEVICE_INFO_PREFIX) +
      base::UTF8ToUTF16(" " + entry->GetDeviceName());
  const gfx::Image notification_icon = gfx::Image();
  const GURL& url = entry->GetURL();
  message_center::RichNotificationData optional_fields;
  // Set the notification to be persistent
  optional_fields.never_timeout = true;

  // Declare a notification
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, entry->GetGUID(),
      base::UTF8ToUTF16(entry->GetTitle()), device_info, notification_icon,
      base::UTF8ToUTF16(url.host()), url, message_center::NotifierId(url),
      optional_fields, nullptr);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::SEND_TAB_TO_SELF, notification);
}

void DesktopNotificationHandler::DismissEntries(
    const std::vector<std::string>& guids) {
  for (const std::string& guid : guids) {
    NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
        NotificationHandler::Type::SEND_TAB_TO_SELF, guid);
  }
}

void DesktopNotificationHandler::OnClose(Profile* profile,
                                         const GURL& origin,
                                         const std::string& notification_id,
                                         bool by_user,
                                         base::OnceClosure completed_closure) {
  SendTabToSelfSyncServiceFactory::GetForProfile(profile)
      ->GetSendTabToSelfModel()
      ->DismissEntry(notification_id);
  std::move(completed_closure).Run();
}

void DesktopNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    base::OnceClosure completed_closure) {
  // Launch a new tab for the notification's |origin|,
  // and close the activated notification.
  NavigateParams params(profile, origin, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      NotificationHandler::Type::SEND_TAB_TO_SELF, notification_id);

  SendTabToSelfSyncServiceFactory::GetForProfile(profile)
      ->GetSendTabToSelfModel()
      ->DeleteEntry(notification_id);

  std::move(completed_closure).Run();
}

}  // namespace send_tab_to_self
