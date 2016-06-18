// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/eol_notification.h"

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
#include "grit/ash_resources.h"
#include "third_party/cros_system_api/dbus/update_engine/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using message_center::MessageCenter;

namespace chromeos {
namespace {

const char kEolNotificationId[] = "eol";
const char kDelegateId[] = "eol_delegate";

class EolNotificationDelegate : public NotificationDelegate {
 public:
  explicit EolNotificationDelegate(Profile* profile);

 private:
  ~EolNotificationDelegate() override;

  // NotificationDelegate overrides:
  void Close(bool by_user) override;
  void ButtonClick(int button_index) override;
  std::string id() const override;

  Profile* const profile_;

  void OpenMoreInfoPage();

  DISALLOW_COPY_AND_ASSIGN(EolNotificationDelegate);
};

EolNotificationDelegate::EolNotificationDelegate(Profile* profile)
    : profile_(profile) {}

EolNotificationDelegate::~EolNotificationDelegate() {}

void EolNotificationDelegate::Close(bool by_user) {
  if (by_user) {
    // set dismiss pref.
    profile_->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, true);
  }
}

void EolNotificationDelegate::ButtonClick(int button_index) {
  // show eol link
  OpenMoreInfoPage();
}

std::string EolNotificationDelegate::id() const {
  return kDelegateId;
}

void EolNotificationDelegate::OpenMoreInfoPage() {
  chrome::NavigateParams params(profile_, GURL(chrome::kEolNotificationURL),
                                ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
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

void EolNotification::OnEolStatus(int status) {
  status_ = status;

  const int pre_eol_status =
      profile_->GetPrefs()->GetInteger(prefs::kEolStatus);
  profile_->GetPrefs()->SetInteger(prefs::kEolStatus, status_);

  if (status_ == update_engine::EndOfLifeStatus::kSupported)
    return;

  if (pre_eol_status != status_) {
    // If Eol status has changed, we should reset
    // kEolNotificationDismissed and show notification.
    profile_->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, false);
  }

  bool user_dismissed_eol_notification =
      profile_->GetPrefs()->GetBoolean(prefs::kEolNotificationDismissed);
  if (user_dismissed_eol_notification)
    return;

  // When device is in Security-Only state, only show notification the first
  // time.
  if (status_ == update_engine::EndOfLifeStatus::kSecurityOnly)
    profile_->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, true);

  Update();
}

void EolNotification::Update() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_EOL_MORE_INFO_BUTTON)));

  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      base::string16(),  // title
      // TODO(xiaoyinh): Update the Eol icon once it's ready.
      GetEolMessage(), bundle.GetImageNamed(IDR_AURA_NOTIFICATION_DISPLAY),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kEolNotificationId),
      base::string16(),  // display_source
      GURL(), kEolNotificationId, data, new EolNotificationDelegate(profile_));
  g_browser_process->notification_ui_manager()->Add(notification, profile_);
}

base::string16 EolNotification::GetEolMessage() {
  if (status_ == update_engine::EndOfLifeStatus::kSecurityOnly) {
    return l10n_util::GetStringUTF16(IDS_EOL_NOTIFICATION_SECURITY_ONLY);
  } else {
    return l10n_util::GetStringUTF16(IDS_EOL_NOTIFICATION_EOL);
  }
}

}  // namespace chromeos
