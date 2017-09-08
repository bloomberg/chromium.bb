// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/eol_notification.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
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
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/message_center_switches.h"

using message_center::MessageCenter;
using l10n_util::GetStringUTF16;

namespace chromeos {
namespace {

const char kEolNotificationId[] = "eol";
const char kDelegateId[] = "eol_delegate";
const SkColor kButtonIconColor = SkColorSetRGB(150, 150, 152);
const SkColor kNotificationIconColor = SkColorSetRGB(219, 68, 55);

class EolNotificationDelegate : public NotificationDelegate {
 public:
  explicit EolNotificationDelegate(Profile* profile);

 private:
  // Buttons that appear in notifications.
  enum EOL_Button { BUTTON_MORE_INFO = 0, BUTTON_DISMISS };

  ~EolNotificationDelegate() override;

  // NotificationDelegate overrides:
  void ButtonClick(int button_index) override;
  std::string id() const override;

  Profile* const profile_;

  void OpenMoreInfoPage();
  void CancelNotification();

  DISALLOW_COPY_AND_ASSIGN(EolNotificationDelegate);
};

EolNotificationDelegate::EolNotificationDelegate(Profile* profile)
    : profile_(profile) {}

EolNotificationDelegate::~EolNotificationDelegate() {}

void EolNotificationDelegate::ButtonClick(int button_index) {
  switch (button_index) {
    case BUTTON_MORE_INFO:
      // show eol link
      OpenMoreInfoPage();
      break;
    case BUTTON_DISMISS:
      // set dismiss pref.
      profile_->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, true);
      break;
    default:
      NOTREACHED();
  }
  CancelNotification();
}

std::string EolNotificationDelegate::id() const {
  return kDelegateId;
}

void EolNotificationDelegate::OpenMoreInfoPage() {
  chrome::NavigateParams params(profile_, GURL(chrome::kEolNotificationURL),
                                ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
}

void EolNotificationDelegate::CancelNotification() {
  // Clean up the notification
  g_browser_process->notification_ui_manager()->CancelById(
      id(), NotificationUIManager::GetProfileID(profile_));
}

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

  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      GetStringUTF16(IDS_EOL_NOTIFICATION_TITLE),
      GetStringUTF16(IDS_EOL_NOTIFICATION_EOL),
      message_center::IsNewStyleNotificationEnabled()
          ? gfx::Image()
          : gfx::Image(CreateVectorIcon(kEolIcon, kNotificationIconColor)),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kEolNotificationId),
      GetStringUTF16(IDS_EOL_NOTIFICATION_DISPLAY_SOURCE), GURL(),
      kEolNotificationId, data, new EolNotificationDelegate(profile_));
  if (message_center::IsNewStyleNotificationEnabled()) {
    notification.set_accent_color(
        message_center::kSystemNotificationColorCriticalWarning);
    notification.set_small_image(gfx::Image(gfx::CreateVectorIcon(
        kNotificationEndOfSupportIcon,
        message_center::kSystemNotificationColorCriticalWarning)));
    notification.set_vector_small_image(kNotificationEndOfSupportIcon);
  }
  g_browser_process->notification_ui_manager()->Add(notification, profile_);
}

}  // namespace chromeos
