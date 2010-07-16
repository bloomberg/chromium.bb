// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_specific_content_settings.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/cookies_tree_model.h"
#include "net/base/cookie_monster.h"

bool TabSpecificContentSettings::IsContentBlocked(
    ContentSettingsType content_type) const {
  DCHECK(content_type != CONTENT_SETTINGS_TYPE_GEOLOCATION)
      << "Geolocation settings handled by ContentSettingGeolocationImageModel";
  DCHECK(content_type != CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
      << "Notifications settings handled by "
      << "ContentSettingsNotificationsImageModel";

  if (content_type == CONTENT_SETTINGS_TYPE_IMAGES ||
      content_type == CONTENT_SETTINGS_TYPE_JAVASCRIPT ||
      content_type == CONTENT_SETTINGS_TYPE_PLUGINS ||
      content_type == CONTENT_SETTINGS_TYPE_COOKIES ||
      content_type == CONTENT_SETTINGS_TYPE_POPUPS)
    return content_blocked_[content_type];

  NOTREACHED();
  return false;
}

void TabSpecificContentSettings::OnContentBlocked(ContentSettingsType type) {
  DCHECK(type != CONTENT_SETTINGS_TYPE_GEOLOCATION)
      << "Geolocation settings handled by OnGeolocationPermissionSet";
  content_blocked_[type] = true;
  if (delegate_)
    delegate_->OnContentSettingsChange();
}

void TabSpecificContentSettings::OnCookieAccessed(
    const GURL& url, const std::string& cookie_line, bool blocked_by_policy) {
  net::CookieOptions options;
  options.set_include_httponly();
  if (blocked_by_policy) {
    blocked_local_shared_objects_.cookies()->SetCookieWithOptions(
        url, cookie_line, options);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  } else {
    allowed_local_shared_objects_.cookies()->SetCookieWithOptions(
        url, cookie_line, options);
  }
}

void TabSpecificContentSettings::OnLocalStorageAccessed(
    const GURL& url, bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.local_storages()->AddLocalStorage(url);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  } else {
    allowed_local_shared_objects_.local_storages()->AddLocalStorage(url);
  }
}

void TabSpecificContentSettings::OnWebDatabaseAccessed(
    const GURL& url,
    const string16& name,
    const string16& display_name,
    unsigned long estimated_size,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.databases()->AddDatabase(
        url, UTF16ToUTF8(name), UTF16ToUTF8(display_name));
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  } else {
    allowed_local_shared_objects_.databases()->AddDatabase(
        url, UTF16ToUTF8(name), UTF16ToUTF8(display_name));
  }
}

void TabSpecificContentSettings::OnAppCacheAccessed(
    const GURL& manifest_url, bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.appcaches()->AddAppCache(manifest_url);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  } else {
    allowed_local_shared_objects_.appcaches()->AddAppCache(manifest_url);
  }
}

void TabSpecificContentSettings::OnGeolocationPermissionSet(
    const GURL& requesting_origin,
    bool allowed) {
  geolocation_settings_state_.OnGeolocationPermissionSet(requesting_origin,
                                                         allowed);
  if (delegate_)
    delegate_->OnContentSettingsChange();
}

TabSpecificContentSettings::TabSpecificContentSettings(
    Delegate* delegate, Profile* profile)
    : allowed_local_shared_objects_(profile),
      blocked_local_shared_objects_(profile),
      geolocation_settings_state_(profile),
      delegate_(NULL) {
  ClearBlockedContentSettings();
  delegate_ = delegate;
}

void TabSpecificContentSettings::ClearBlockedContentSettings() {
  for (size_t i = 0; i < arraysize(content_blocked_); ++i)
    content_blocked_[i] = false;
  blocked_local_shared_objects_.Reset();
  allowed_local_shared_objects_.Reset();
  if (delegate_)
    delegate_->OnContentSettingsChange();
}

void TabSpecificContentSettings::SetPopupsBlocked(bool blocked) {
  content_blocked_[CONTENT_SETTINGS_TYPE_POPUPS] = blocked;
  if (delegate_)
    delegate_->OnContentSettingsChange();
}

void TabSpecificContentSettings::GeolocationDidNavigate(
      const NavigationController::LoadCommittedDetails& details) {
  geolocation_settings_state_.DidNavigate(details);
}

CookiesTreeModel* TabSpecificContentSettings::GetAllowedCookiesTreeModel() {
  return allowed_local_shared_objects_.GetCookiesTreeModel();
}

CookiesTreeModel* TabSpecificContentSettings::GetBlockedCookiesTreeModel() {
  return blocked_local_shared_objects_.GetCookiesTreeModel();
}

TabSpecificContentSettings::LocalSharedObjectsContainer::
    LocalSharedObjectsContainer(Profile* profile)
    : cookies_(new net::CookieMonster(NULL, NULL)),
      appcaches_(new CannedBrowsingDataAppCacheHelper(profile)),
      databases_(new CannedBrowsingDataDatabaseHelper(profile)),
      local_storages_(new CannedBrowsingDataLocalStorageHelper(profile)) {
}

TabSpecificContentSettings::LocalSharedObjectsContainer::
    ~LocalSharedObjectsContainer() {
}

void TabSpecificContentSettings::LocalSharedObjectsContainer::Reset() {
  cookies_->DeleteAll(false);
  appcaches_->Reset();
  databases_->Reset();
  local_storages_->Reset();
}

CookiesTreeModel*
TabSpecificContentSettings::LocalSharedObjectsContainer::GetCookiesTreeModel() {
  return new CookiesTreeModel(
      cookies_, databases_, local_storages_, appcaches_);
}
