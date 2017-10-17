// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/eol_notification.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/message_center_switches.h"

using message_center::MessageCenter;
using l10n_util::GetStringUTF16;

namespace chromeos {
namespace {

const char kEolNotificationId[] = "chrome://product_eol";
const SkColor kButtonIconColor = SkColorSetRGB(150, 150, 152);
const SkColor kNotificationIconColor = SkColorSetRGB(219, 68, 55);

class EolNotificationHandler : public NotificationHandler {
 public:
  ~EolNotificationHandler() override = default;

  void OnClick(Profile* profile,
               const std::string& origin,
               const std::string& notification_id,
               const base::Optional<int>& action_index,
               const base::Optional<base::string16>& reply) override {
    switch (*action_index) {
      case BUTTON_MORE_INFO: {
        // show eol link
        chrome::NavigateParams params(profile,
                                      GURL(chrome::kEolNotificationURL),
                                      ui::PAGE_TRANSITION_LINK);
        params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
        params.window_action = chrome::NavigateParams::SHOW_WINDOW;
        chrome::Navigate(&params);
        break;
      }
      case BUTTON_DISMISS:
        // set dismiss pref.
        profile->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, true);
        break;
    }
    NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
        NotificationCommon::PRODUCT_EOL, kEolNotificationId);
  }

  static void Register(Profile* profile) {
    NotificationDisplayService* service =
        NotificationDisplayServiceFactory::GetForProfile(profile);
    if (!service->GetNotificationHandler(NotificationCommon::PRODUCT_EOL)) {
      service->AddNotificationHandler(
          NotificationCommon::PRODUCT_EOL,
          base::WrapUnique(new EolNotificationHandler()));
    }
  }

 private:
  EolNotificationHandler() = default;

  // Buttons that appear in notifications.
  enum { BUTTON_MORE_INFO = 0, BUTTON_DISMISS };

  DISALLOW_COPY_AND_ASSIGN(EolNotificationHandler);
};

}  // namespace

EolNotification::EolNotification(Profile* profile)
    : profile_(profile),
      status_(update_engine::EndOfLifeStatus::kSupported),
      weak_factory_(this) {}

EolNotification::~EolNotification() {}

void EolNotification::CheckEolStatus() {
  UpdateEngineClient* update_engine_client =
      DBusThreadManager::Get()->GetUpdateEngineClient();

  // Request the Eol Status.
  update_engine_client->GetEolStatus(
      base::Bind(&EolNotification::OnEolStatus, weak_factory_.GetWeakPtr()));
}

void EolNotification::OnEolStatus(update_engine::EndOfLifeStatus status) {
  status_ = status;

  const int pre_eol_status =
      profile_->GetPrefs()->GetInteger(prefs::kEolStatus);
  profile_->GetPrefs()->SetInteger(prefs::kEolStatus, status_);

  // Security only state is no longer supported.
  if (status_ == update_engine::EndOfLifeStatus::kSupported ||
      status_ == update_engine::EndOfLifeStatus::kSecurityOnly) {
    return;
  }

  if (pre_eol_status != status_) {
    // If Eol status has changed, we should reset
    // kEolNotificationDismissed and show notification.
    profile_->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, false);
  }

  bool user_dismissed_eol_notification =
      profile_->GetPrefs()->GetBoolean(prefs::kEolNotificationDismissed);
  if (user_dismissed_eol_notification)
    return;

  Update();
}

void EolNotification::Update() {
  message_center::ButtonInfo learn_more(
      GetStringUTF16(IDS_EOL_MORE_INFO_BUTTON));
  learn_more.icon = gfx::Image(
      CreateVectorIcon(vector_icons::kInfoOutlineIcon, kButtonIconColor));
  message_center::ButtonInfo dismiss(GetStringUTF16(IDS_EOL_DISMISS_BUTTON));
  dismiss.icon = gfx::Image(
      CreateVectorIcon(vector_icons::kNotificationsOffIcon, kButtonIconColor));

  message_center::RichNotificationData data;
  data.buttons.push_back(learn_more);
  data.buttons.push_back(dismiss);

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kEolNotificationId,
      GetStringUTF16(IDS_EOL_NOTIFICATION_TITLE),
      GetStringUTF16(IDS_EOL_NOTIFICATION_EOL),
      message_center::IsNewStyleNotificationEnabled()
          ? gfx::Image()
          : gfx::Image(CreateVectorIcon(kEolIcon, kNotificationIconColor)),
      GetStringUTF16(IDS_EOL_NOTIFICATION_DISPLAY_SOURCE),
      GURL(kEolNotificationId),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kEolNotificationId),
      data, nullptr);

  if (message_center::IsNewStyleNotificationEnabled()) {
    notification.set_accent_color(
        message_center::kSystemNotificationColorCriticalWarning);
    notification.set_small_image(gfx::Image(gfx::CreateVectorIcon(
        kNotificationEndOfSupportIcon,
        message_center::kSystemNotificationColorCriticalWarning)));
    notification.set_vector_small_image(kNotificationEndOfSupportIcon);
  }

  EolNotificationHandler::Register(profile_);
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationCommon::PRODUCT_EOL, kEolNotificationId, notification);
}

}  // namespace chromeos
