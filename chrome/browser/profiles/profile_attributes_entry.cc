// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {
bool IsForceSigninEnabled() {
  PrefService* prefs = g_browser_process->local_state();
  return prefs && prefs->GetBoolean(prefs::kForceBrowserSignin);
}
}  // namespace

ProfileAttributesEntry::ProfileAttributesEntry()
    : profile_info_cache_(nullptr), profile_path_(base::FilePath()) {}

void ProfileAttributesEntry::Initialize(
    ProfileInfoCache* cache, const base::FilePath& path) {
  DCHECK(!profile_info_cache_);
  DCHECK(cache);
  profile_info_cache_ = cache;
  DCHECK(profile_path_.empty());
  DCHECK(!path.empty());
  profile_path_ = path;
  is_force_signin_enabled_ = IsForceSigninEnabled();
  if (!IsAuthenticated() && is_force_signin_enabled_)
    is_force_signin_profile_locked_ = true;
}

base::string16 ProfileAttributesEntry::GetName() const {
  return profile_info_cache_->GetNameOfProfileAtIndex(profile_index());
}

base::string16 ProfileAttributesEntry::GetShortcutName() const {
  return profile_info_cache_->GetShortcutNameOfProfileAtIndex(profile_index());
}

base::FilePath ProfileAttributesEntry::GetPath() const {
  return profile_info_cache_->GetPathOfProfileAtIndex(profile_index());
}

base::Time ProfileAttributesEntry::GetActiveTime() const {
  return profile_info_cache_->GetProfileActiveTimeAtIndex(profile_index());
}

base::string16 ProfileAttributesEntry::GetUserName() const {
  return profile_info_cache_->GetUserNameOfProfileAtIndex(profile_index());
}

const gfx::Image& ProfileAttributesEntry::GetAvatarIcon() const {
  return profile_info_cache_->GetAvatarIconOfProfileAtIndex(profile_index());
}

std::string ProfileAttributesEntry::GetLocalAuthCredentials() const {
  return profile_info_cache_->GetLocalAuthCredentialsOfProfileAtIndex(
      profile_index());
}

std::string ProfileAttributesEntry::GetPasswordChangeDetectionToken() const {
  return profile_info_cache_->GetPasswordChangeDetectionTokenAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::GetBackgroundStatus() const {
  return profile_info_cache_->GetBackgroundStatusOfProfileAtIndex(
      profile_index());
}

base::string16 ProfileAttributesEntry::GetGAIAName() const {
  return profile_info_cache_->GetGAIANameOfProfileAtIndex(profile_index());
}

base::string16 ProfileAttributesEntry::GetGAIAGivenName() const {
  return profile_info_cache_->GetGAIAGivenNameOfProfileAtIndex(profile_index());
}

std::string ProfileAttributesEntry::GetGAIAId() const {
  return profile_info_cache_->GetGAIAIdOfProfileAtIndex(profile_index());
}

const gfx::Image* ProfileAttributesEntry::GetGAIAPicture() const {
  return profile_info_cache_->GetGAIAPictureOfProfileAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsUsingGAIAPicture() const {
  return profile_info_cache_->IsUsingGAIAPictureOfProfileAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::IsSupervised() const {
  return profile_info_cache_->ProfileIsSupervisedAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsChild() const {
  return profile_info_cache_->ProfileIsChildAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsLegacySupervised() const {
  return profile_info_cache_->ProfileIsLegacySupervisedAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsOmitted() const {
  return profile_info_cache_->IsOmittedProfileAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsSigninRequired() const {
  return profile_info_cache_->ProfileIsSigninRequiredAtIndex(profile_index()) ||
         is_force_signin_profile_locked_;
}

std::string ProfileAttributesEntry::GetSupervisedUserId() const {
  return profile_info_cache_->GetSupervisedUserIdOfProfileAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::IsEphemeral() const {
  return profile_info_cache_->ProfileIsEphemeralAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsUsingDefaultName() const {
  return profile_info_cache_->ProfileIsUsingDefaultNameAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsAuthenticated() const {
  return profile_info_cache_->ProfileIsAuthenticatedAtIndex(profile_index());
}

bool ProfileAttributesEntry::IsUsingDefaultAvatar() const {
  return profile_info_cache_->ProfileIsUsingDefaultAvatarAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::IsAuthError() const {
  return profile_info_cache_->ProfileIsAuthErrorAtIndex(profile_index());
}

size_t ProfileAttributesEntry::GetAvatarIconIndex() const {
  return profile_info_cache_->GetAvatarIconIndexOfProfileAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::HasStatsBrowsingHistory() const {
  return profile_info_cache_->HasStatsBrowsingHistoryOfProfileAtIndex(
      profile_index());
}

int ProfileAttributesEntry::GetStatsBrowsingHistory() const {
  return profile_info_cache_->GetStatsBrowsingHistoryOfProfileAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::HasStatsPasswords() const {
  return profile_info_cache_->HasStatsPasswordsOfProfileAtIndex(
      profile_index());
}

int ProfileAttributesEntry::GetStatsPasswords() const {
  return profile_info_cache_->GetStatsPasswordsOfProfileAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::HasStatsBookmarks() const {
  return profile_info_cache_->HasStatsBookmarksOfProfileAtIndex(
      profile_index());
}

int ProfileAttributesEntry::GetStatsBookmarks() const {
  return profile_info_cache_->GetStatsBookmarksOfProfileAtIndex(
      profile_index());
}

bool ProfileAttributesEntry::HasStatsSettings() const {
  return profile_info_cache_->HasStatsSettingsOfProfileAtIndex(profile_index());
}

int ProfileAttributesEntry::GetStatsSettings() const {
  return profile_info_cache_->GetStatsSettingsOfProfileAtIndex(profile_index());
}

void ProfileAttributesEntry::SetName(const base::string16& name) {
  profile_info_cache_->SetNameOfProfileAtIndex(profile_index(), name);
}

void ProfileAttributesEntry::SetShortcutName(const base::string16& name) {
  profile_info_cache_->SetShortcutNameOfProfileAtIndex(profile_index(), name);
}

void ProfileAttributesEntry::SetActiveTimeToNow() {
  profile_info_cache_->SetProfileActiveTimeAtIndex(profile_index());
}

void ProfileAttributesEntry::SetIsOmitted(bool is_omitted) {
  profile_info_cache_->SetIsOmittedProfileAtIndex(profile_index(), is_omitted);
}

void ProfileAttributesEntry::SetSupervisedUserId(const std::string& id) {
  profile_info_cache_->SetSupervisedUserIdOfProfileAtIndex(profile_index(), id);
}

void ProfileAttributesEntry::SetLocalAuthCredentials(const std::string& auth) {
  profile_info_cache_->SetLocalAuthCredentialsOfProfileAtIndex(
      profile_index(), auth);
}

void ProfileAttributesEntry::SetPasswordChangeDetectionToken(
    const std::string& token) {
  profile_info_cache_->SetPasswordChangeDetectionTokenAtIndex(
      profile_index(), token);
}

void ProfileAttributesEntry::SetBackgroundStatus(bool running_background_apps) {
  profile_info_cache_->SetBackgroundStatusOfProfileAtIndex(
      profile_index(), running_background_apps);
}

void ProfileAttributesEntry::SetGAIAName(const base::string16& name) {
  profile_info_cache_->SetGAIANameOfProfileAtIndex(profile_index(), name);
}

void ProfileAttributesEntry::SetGAIAGivenName(const base::string16& name) {
  profile_info_cache_->SetGAIAGivenNameOfProfileAtIndex(profile_index(), name);
}

void ProfileAttributesEntry::SetGAIAPicture(const gfx::Image* image) {
  profile_info_cache_->SetGAIAPictureOfProfileAtIndex(profile_index(), image);
}

void ProfileAttributesEntry::SetIsUsingGAIAPicture(bool value) {
  profile_info_cache_->SetIsUsingGAIAPictureOfProfileAtIndex(
      profile_index(), value);
}

void ProfileAttributesEntry::SetIsSigninRequired(bool value) {
  profile_info_cache_->SetProfileSigninRequiredAtIndex(profile_index(), value);
  if (is_force_signin_enabled_)
    LockForceSigninProfile(value);
}

void ProfileAttributesEntry::LockForceSigninProfile(bool is_lock) {
  DCHECK(is_force_signin_enabled_);
  is_force_signin_profile_locked_ = is_lock;
}

void ProfileAttributesEntry::SetIsEphemeral(bool value) {
  profile_info_cache_->SetProfileIsEphemeralAtIndex(profile_index(), value);
}

void ProfileAttributesEntry::SetIsUsingDefaultName(bool value) {
  profile_info_cache_->SetProfileIsUsingDefaultNameAtIndex(
      profile_index(), value);
}

void ProfileAttributesEntry::SetIsUsingDefaultAvatar(bool value) {
  profile_info_cache_->SetProfileIsUsingDefaultAvatarAtIndex(
      profile_index(), value);
}

void ProfileAttributesEntry::SetIsAuthError(bool value) {
  profile_info_cache_->SetProfileIsAuthErrorAtIndex(profile_index(), value);
}

void ProfileAttributesEntry::SetAvatarIconIndex(size_t icon_index) {
  profile_info_cache_->SetAvatarIconOfProfileAtIndex(
      profile_index(), icon_index);
}

void ProfileAttributesEntry::SetStatsBrowsingHistory(int value) {
  profile_info_cache_->SetStatsBrowsingHistoryOfProfileAtIndex(profile_index(),
                                                               value);
}

void ProfileAttributesEntry::SetStatsPasswords(int value) {
  profile_info_cache_->SetStatsPasswordsOfProfileAtIndex(profile_index(),
                                                         value);
}

void ProfileAttributesEntry::SetStatsBookmarks(int value) {
  profile_info_cache_->SetStatsBookmarksOfProfileAtIndex(profile_index(),
                                                         value);
}

void ProfileAttributesEntry::SetStatsSettings(int value) {
  profile_info_cache_->SetStatsSettingsOfProfileAtIndex(profile_index(), value);
}

void ProfileAttributesEntry::SetAuthInfo(
    const std::string& gaia_id, const base::string16& user_name) {
  profile_info_cache_->SetAuthInfoOfProfileAtIndex(
      profile_index(), gaia_id, user_name);
}

size_t ProfileAttributesEntry::profile_index() const {
  size_t index = profile_info_cache_->GetIndexOfProfileWithPath(profile_path_);
  DCHECK(index < profile_info_cache_->GetNumberOfProfiles());
  return index;
}
