// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_specific_content_settings.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/cookies_tree_model.h"
#include "net/base/cookie_monster.h"

bool TabSpecificContentSettings::LocalSharedObjectsContainer::empty() const {
  return cookies_->GetAllCookies().empty() &&
      appcaches_->empty() &&
      databases_->empty() &&
      indexed_dbs_->empty() &&
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

bool TabSpecificContentSettings::IsBlockageIndicated(
    ContentSettingsType content_type) const {
  return content_blockage_indicated_to_user_[content_type];
}

void TabSpecificContentSettings::SetBlockageHasBeenIndicated(
    ContentSettingsType content_type) {
  content_blockage_indicated_to_user_[content_type] = true;
}

bool TabSpecificContentSettings::IsContentAccessed(
    ContentSettingsType content_type) const {
  // This method currently only returns meaningful values for cookies.
  if (content_type != CONTENT_SETTINGS_TYPE_COOKIES)
    return false;

  return content_accessed_[content_type];
}

const std::set<std::string>&
    TabSpecificContentSettings::BlockedResourcesForType(
        ContentSettingsType content_type) const {
  if (blocked_resources_[content_type].get()) {
    return *blocked_resources_[content_type];
  } else {
    static std::set<std::string> empty_set;
    return empty_set;
  }
}

void TabSpecificContentSettings::AddBlockedResource(
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  if (!blocked_resources_[content_type].get())
    blocked_resources_[content_type].reset(new std::set<std::string>());
  blocked_resources_[content_type]->insert(resource_identifier);
}

void TabSpecificContentSettings::OnContentBlocked(
    ContentSettingsType type,
    const std::string& resource_identifier) {
  DCHECK(type != CONTENT_SETTINGS_TYPE_GEOLOCATION)
      << "Geolocation settings handled by OnGeolocationPermissionSet";
  content_accessed_[type] = true;
  if (!resource_identifier.empty())
    AddBlockedResource(type, resource_identifier);
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

void TabSpecificContentSettings::OnCookiesRead(
    const GURL& url,
    const net::CookieList& cookie_list,
    bool blocked_by_policy) {
  if (cookie_list.empty())
    return;
  LocalSharedObjectsContainer& container = blocked_by_policy ?
      blocked_local_shared_objects_ : allowed_local_shared_objects_;
  typedef net::CookieList::const_iterator cookie_iterator;
  for (cookie_iterator cookie = cookie_list.begin();
       cookie != cookie_list.end(); ++cookie) {
    container.cookies()->ValidateMap();
    container.cookies()->SetCookieWithDetails(url,
                                              cookie->Name(),
                                              cookie->Value(),
                                              cookie->Domain(),
                                              cookie->Path(),
                                              cookie->ExpiryDate(),
                                              cookie->IsSecure(),
                                              cookie->IsHttpOnly());
    container.cookies()->ValidateMap();
  }
  if (blocked_by_policy)
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
  else
    OnContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES);
}

void TabSpecificContentSettings::OnCookieChanged(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.cookies()->ValidateMap();
    blocked_local_shared_objects_.cookies()->SetCookieWithOptions(
        url, cookie_line, options);
    blocked_local_shared_objects_.cookies()->ValidateMap();
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
  } else {
    allowed_local_shared_objects_.cookies()->ValidateMap();
    allowed_local_shared_objects_.cookies()->SetCookieWithOptions(
        url, cookie_line, options);
    allowed_local_shared_objects_.cookies()->ValidateMap();
    OnContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES);
  }
}

void TabSpecificContentSettings::OnIndexedDBAccessed(
    const GURL& url,
    const string16& description,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.indexed_dbs()->AddIndexedDB(
        url, description);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
  }else {
    allowed_local_shared_objects_.indexed_dbs()->AddIndexedDB(
        url, description);
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
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
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
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
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
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
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
  ClearBlockedContentSettingsExceptForCookies();
  ClearCookieSpecificContentSettings();
  delegate_ = delegate;
}

void TabSpecificContentSettings::ClearBlockedContentSettingsExceptForCookies() {
  for (size_t i = 0; i < arraysize(content_blocked_); ++i) {
    if (i == CONTENT_SETTINGS_TYPE_COOKIES)
      continue;
    blocked_resources_[i].reset();
    content_blocked_[i] = false;
    content_accessed_[i] = false;
    content_blockage_indicated_to_user_[i] = false;
  }
  load_plugins_link_enabled_ = true;
  if (delegate_)
    delegate_->OnContentSettingsAccessed(false);
}

void TabSpecificContentSettings::ClearCookieSpecificContentSettings() {
  blocked_local_shared_objects_.Reset();
  allowed_local_shared_objects_.Reset();
  content_blocked_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  content_accessed_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  content_blockage_indicated_to_user_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  if (delegate_)
    delegate_->OnContentSettingsAccessed(false);
}

void TabSpecificContentSettings::SetPopupsBlocked(bool blocked) {
  content_blocked_[CONTENT_SETTINGS_TYPE_POPUPS] = blocked;
  content_blockage_indicated_to_user_[CONTENT_SETTINGS_TYPE_POPUPS] = false;
  if (delegate_)
    delegate_->OnContentSettingsAccessed(blocked);
}

void TabSpecificContentSettings::GeolocationDidNavigate(
      const NavigationController::LoadCommittedDetails& details) {
  geolocation_settings_state_.DidNavigate(details);
}

void TabSpecificContentSettings::ClearGeolocationContentSettings() {
  geolocation_settings_state_.ClearStateMap();
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
      indexed_dbs_(new CannedBrowsingDataIndexedDBHelper(profile)),
      local_storages_(new CannedBrowsingDataLocalStorageHelper(profile)),
      session_storages_(new CannedBrowsingDataLocalStorageHelper(profile)) {
}

TabSpecificContentSettings::LocalSharedObjectsContainer::
    ~LocalSharedObjectsContainer() {
  cookies_->ValidateMap();
}

void TabSpecificContentSettings::LocalSharedObjectsContainer::Reset() {
  cookies_->DeleteAll(false);
  appcaches_->Reset();
  databases_->Reset();
  indexed_dbs_->Reset();
  local_storages_->Reset();
  session_storages_->Reset();
}

CookiesTreeModel*
TabSpecificContentSettings::LocalSharedObjectsContainer::GetCookiesTreeModel() {
  return new CookiesTreeModel(cookies_,
                              databases_,
                              local_storages_,
                              session_storages_,
                              appcaches_,
                              indexed_dbs_);
}
