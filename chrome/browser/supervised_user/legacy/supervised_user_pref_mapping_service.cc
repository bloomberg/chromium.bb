// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/legacy/supervised_user_pref_mapping_service.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

const int kNoAvatar = -1;

SupervisedUserPrefMappingService::SupervisedUserPrefMappingService(
    PrefService* prefs,
    SupervisedUserSharedSettingsService* shared_settings)
    : prefs_(prefs),
      shared_settings_(shared_settings),
      supervised_user_id_(prefs->GetString(prefs::kSupervisedUserId)),
      weak_ptr_factory_(this) {}

SupervisedUserPrefMappingService::~SupervisedUserPrefMappingService() {}

void SupervisedUserPrefMappingService::Init() {
  subscription_ = shared_settings_->Subscribe(
      base::Bind(&SupervisedUserPrefMappingService::OnSharedSettingChanged,
                 weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(
      prefs::kProfileAvatarIndex,
      base::Bind(&SupervisedUserPrefMappingService::OnAvatarChanged,
                 weak_ptr_factory_.GetWeakPtr()));

  // Check if we need to update the shared setting with the avatar index.
  // Otherwise we update the user pref in case we missed a notification.
  if (GetChromeAvatarIndex() == kNoAvatar) {
    OnAvatarChanged();
  } else {
    OnSharedSettingChanged(supervised_user_id_,
                           supervised_users::kChromeAvatarIndex);
  }
}

void SupervisedUserPrefMappingService::OnAvatarChanged() {
  int new_avatar_index = prefs_->GetInteger(prefs::kProfileAvatarIndex);
  if (new_avatar_index < 0)
    return;

  // First check if the avatar index is a new value.
  if (GetChromeAvatarIndex() == new_avatar_index)
    return;

  // If yes, update the shared settings value.
  shared_settings_->SetValue(supervised_user_id_,
                             supervised_users::kChromeAvatarIndex,
                             base::FundamentalValue(new_avatar_index));
}

void SupervisedUserPrefMappingService::OnSharedSettingChanged(
    const std::string& su_id,
    const std::string& key) {
  DCHECK_EQ(su_id, supervised_user_id_);
  if (key != supervised_users::kChromeAvatarIndex)
    return;

  const base::Value* value = shared_settings_->GetValue(su_id, key);
  int avatar_index;
  bool success = value->GetAsInteger(&avatar_index);
  DCHECK(success);
  prefs_->SetInteger(prefs::kProfileAvatarIndex, avatar_index);
}

void SupervisedUserPrefMappingService::Shutdown() {
  subscription_.reset();
}

int SupervisedUserPrefMappingService::GetChromeAvatarIndex() {
  const base::Value* value = shared_settings_->GetValue(
      supervised_user_id_, supervised_users::kChromeAvatarIndex);
  if (!value)
    return kNoAvatar;

  int current_avatar_index;
  bool success = value->GetAsInteger(&current_avatar_index);
  DCHECK(success);
  return current_avatar_index;
}
