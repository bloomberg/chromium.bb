// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/multi_profile_first_run_notification.h"

#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

using message_center::Notification;

namespace chromeos {

namespace {

const char kNotificationId[] = "chrome:://login/multiprofile";

}  // namespace

MultiProfileFirstRunNotification::MultiProfileFirstRunNotification()
    : weak_ptr_factory_(this) {}

MultiProfileFirstRunNotification::~MultiProfileFirstRunNotification() {}

// static
void MultiProfileFirstRunNotification::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kMultiProfileNotificationDismissed,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void MultiProfileFirstRunNotification::UserProfilePrepared(
    Profile* user_profile) {
  if (!UserManager::IsMultipleProfilesAllowed() ||
      UserManager::Get()->GetLoggedInUsers().size() > 1 ||
      user_profile->GetPrefs()->GetBoolean(
          prefs::kMultiProfileNotificationDismissed)) {
    return;
  }

  const base::string16 title;
  const base::string16 display_source;
  scoped_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kNotificationId,
      title,
      l10n_util::GetStringUTF16(IDS_MULTI_PROFILES_WARNING),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_ALERT),
      display_source,
      message_center::NotifierId(),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickedDelegate(
          base::Bind(&MultiProfileFirstRunNotification::OnDismissed,
                     weak_ptr_factory_.GetWeakPtr(),
                     user_profile))));
  notification->SetSystemPriority();
  message_center::MessageCenter::Get()->AddNotification(notification.Pass());
}

void MultiProfileFirstRunNotification::OnDismissed(Profile* user_profile) {
  user_profile->GetPrefs()->SetBoolean(
      prefs::kMultiProfileNotificationDismissed, true);
}

}  // namespace chromeos
