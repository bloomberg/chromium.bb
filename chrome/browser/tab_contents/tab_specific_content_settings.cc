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

bool TabSpecificContentSettings::LocalSharedObjectsContainer::empty() const {
  return cookies_->GetAllCookies().empty() &&
      appcaches_->empty() &&
      databases_->empty() &&
      local_storages_->empty() &&
      session_storages_->empty();
}

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

bool TabSpecificContentSettings::IsContentAccessed(
    ContentSettingsType content_type) const {
  // This method currently only returns meaningful values for cookies.
  if (content_type != CONTENT_SETTINGS_TYPE_COOKIES)
    return false;

  return content_accessed_[content_type];
}

void TabSpecificContentSettings::OnContentBlocked(ContentSettingsType type) {
  DCHECK(type != CONTENT_SETTINGS_TYPE_GEOLOCATION)
      << "Geolocation settings handled by OnGeolocationPermissionSet";
  content_accessed_[type] = true;
  if (!content_blocked_[type]) {
    content_blocked_[type] = true;
    if (delegate_)
      delegate_->OnContentSettingsAccessed(true);
  }
}

void TabSpecificContentSettings::OnContentAccessed(ContentSettingsType type) {
  DCHECK(type != CONTENT_SETTINGS_TYPE_GEOLOCATION)
      << "Geolocation settings handled by OnGeolocationPermissionSet";
  if (!content_accessed_[type]) {
    content_accessed_[type] = true;
    if (delegate_)
      delegate_->OnContentSettingsAccessed(false);
  }
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
    OnContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES);
  }
}

void TabSpecificContentSettings::OnLocalStorageAccessed(
    const GURL& url,
    DOMStorageType storage_type,
    bool blocked_by_policy) {
  LocalSharedObjectsContainer& container = blocked_by_policy ?
      blocked_local_shared_objects_ : allowed_local_shared_objects_;
  CannedBrowsingDataLocalStorageHelper* helper =
      storage_type == DOM_STORAGE_LOCAL ?
          container.local_storages() : container.session_storages();
  helper->AddLocalStorage(url);

  if (blocked_by_policy)
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  else
    OnContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES);
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
    OnContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES);
  }
}

void TabSpecificContentSettings::OnAppCacheAccessed(
    const GURL& manifest_url, bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.appcaches()->AddAppCache(manifest_url);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);
  } else {
    allowed_local_shared_objects_.appcaches()->AddAppCache(manifest_url);
    OnContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES);
  }
}

void TabSpecificContentSettings::OnGeolocationPermissionSet(
    const GURL& requesting_origin,
    bool allowed) {
  geolocation_settings_state_.OnGeolocationPermissionSet(requesting_origin,
                                                         allowed);
  if (delegate_)
    delegate_->OnContentSettingsAccessed(!allowed);
}

TabSpecificContentSettings::TabSpecificContentSettings(
    Delegate* delegate, Profile* profile)
    : allowed_local_shared_objects_(profile),
      blocked_local_shared_objects_(profile),
      geolocation_settings_state_(profile),
      load_plugins_link_enabled_(true),
      delegate_(NULL) {
  ClearBlockedContentSettings();
  delegate_ = delegate;
}

void TabSpecificContentSettings::ClearBlockedContentSettings() {
  for (size_t i = 0; i < arraysize(content_blocked_); ++i) {
    content_blocked_[i] = false;
    content_accessed_[i] = false;
  }
  load_plugins_link_enabled_ = true;
  blocked_local_shared_objects_.Reset();
  allowed_local_shared_objects_.Reset();
  if (delegate_)
    delegate_->OnContentSettingsAccessed(false);
}

void TabSpecificContentSettings::SetPopupsBlocked(bool blocked) {
  content_blocked_[CONTENT_SETTINGS_TYPE_POPUPS] = blocked;
  if (delegate_)
    delegate_->OnContentSettingsAccessed(blocked);
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
      local_storages_(new CannedBrowsingDataLocalStorageHelper(profile)),
      session_storages_(new CannedBrowsingDataLocalStorageHelper(profile)) {
}

TabSpecificContentSettings::LocalSharedObjectsContainer::
    ~LocalSharedObjectsContainer() {
}

void TabSpecificContentSettings::LocalSharedObjectsContainer::Reset() {
  cookies_->DeleteAll(false);
  appcaches_->Reset();
  databases_->Reset();
  local_storages_->Reset();
  session_storages_->Reset();
}

CookiesTreeModel*
TabSpecificContentSettings::LocalSharedObjectsContainer::GetCookiesTreeModel() {
  return new CookiesTreeModel(
      cookies_, databases_, local_storages_, session_storages_, appcaches_);
}
