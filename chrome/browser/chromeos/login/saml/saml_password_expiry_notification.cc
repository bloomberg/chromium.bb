// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/saml_password_expiry_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/chrome_new_window_client.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::HandleNotificationClickDelegate;
using message_center::Notification;
using message_center::NotificationType;
using message_center::NotifierId;
using message_center::NotifierType;
using message_center::RichNotificationData;
using message_center::SystemNotificationWarningLevel;

namespace chromeos {

namespace {

// Unique ID for this notification.
const char kNotificationId[] = "saml.password-expiry-notification";

// NotifierId for histogram reporting.
const base::NoDestructor<NotifierId> kNotifierId(
    message_center::NotifierType::SYSTEM_COMPONENT,
    kNotificationId);

// Simplest type of notification - has text but no other UI elements.
const NotificationType kNotificationType =
    message_center::NOTIFICATION_TYPE_SIMPLE;

// Generic type for notifications that are not from web pages etc.
const NotificationHandler::Type kNotificationHandlerType =
    NotificationHandler::Type::TRANSIENT;

// The icon to use for this notification - looks like an office building.
const gfx::VectorIcon& kIcon = vector_icons::kBusinessIcon;

// Warning level NORMAL means the notification heading is blue.
const SystemNotificationWarningLevel kWarningLevel =
    SystemNotificationWarningLevel::NORMAL;

// Leaving this empty means the notification is attributed to the system -
// ie "Chromium OS" or similar.
const base::NoDestructor<base::string16> kDisplaySource;

// These extra attributes aren't needed, so they are left empty.
const base::NoDestructor<GURL> kOriginUrl;
const base::NoDestructor<RichNotificationData> kRichNotificationData;

// Line separator in the notification body.
const base::NoDestructor<base::string16> kLineSeparator(
    base::string16(1, '\n'));

// Callback called when notification is clicked - opens password-change page.
void OnNotificationClicked() {
  ChromeNewWindowClient* client = ChromeNewWindowClient::Get();
  if (client) {
    client->NewTabWithUrl(GURL(chrome::kChromeUIPasswordChangeUrl),
                          /*from_user_interaction=*/true);
  }
}

const base::NoDestructor<scoped_refptr<HandleNotificationClickDelegate>>
    kClickDelegate(base::MakeRefCounted<HandleNotificationClickDelegate>(
        base::BindRepeating(&OnNotificationClicked)));

base::string16 GetTitleText(int lessThanNDays) {
  const bool hasExpired = (lessThanNDays <= 0);
  return hasExpired ? l10n_util::GetStringUTF16(IDS_PASSWORD_HAS_EXPIRED_TITLE)
                    : l10n_util::GetStringUTF16(IDS_PASSWORD_WILL_EXPIRE_TITLE);
}

base::string16 GetBodyText(int lessThanNDays) {
  const std::vector<base::string16> body_lines = {
      l10n_util::GetPluralStringFUTF16(IDS_PASSWORD_EXPIRY_DAYS_BODY,
                                       std::max(lessThanNDays, 0)),
      l10n_util::GetStringUTF16(IDS_PASSWORD_EXPIRY_CHOOSE_NEW_PASSWORD_LINK)};

  return base::JoinString(body_lines, *kLineSeparator);
}

}  // namespace

void ShowSamlPasswordExpiryNotification(Profile* profile, int lessThanNDays) {
  const base::string16 title = GetTitleText(lessThanNDays);
  const base::string16 body = GetBodyText(lessThanNDays);

  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      kNotificationType, kNotificationId, title, body, *kDisplaySource,
      *kOriginUrl, *kNotifierId, *kRichNotificationData, *kClickDelegate, kIcon,
      kWarningLevel);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      kNotificationHandlerType, *notification, /*metadata=*/nullptr);
}

void DismissSamlPasswordExpiryNotification(Profile* profile) {
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      kNotificationHandlerType, kNotificationId);
}

}  // namespace chromeos
