// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/tab_specific_content_settings.h"

#include <list>

#include "base/lazy_instance.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "net/base/cookie_monster.h"

namespace {
typedef std::list<TabSpecificContentSettings*> TabSpecificList;
static base::LazyInstance<TabSpecificList> g_tab_specific(
    base::LINKER_INITIALIZED);
}

bool TabSpecificContentSettings::LocalSharedObjectsContainer::empty() const {
  return cookies_->GetAllCookies().empty() &&
      appcaches_->empty() &&
      databases_->empty() &&
      indexed_dbs_->empty() &&
      local_storages_->empty() &&
      session_storages_->empty();
}

TabSpecificContentSettings::TabSpecificContentSettings(TabContents* tab)
    : TabContentsObserver(tab),
      allowed_local_shared_objects_(tab->profile()),
      blocked_local_shared_objects_(tab->profile()),
      geolocation_settings_state_(tab->profile()),
      load_plugins_link_enabled_(true) {
  ClearBlockedContentSettingsExceptForCookies();
  ClearCookieSpecificContentSettings();
  g_tab_specific.Get().push_back(this);

  registrar_.Add(this, NotificationType::CONTENT_SETTINGS_CHANGED,
                 NotificationService::AllSources());
}

TabSpecificContentSettings::~TabSpecificContentSettings() {
  g_tab_specific.Get().remove(this);
}

TabSpecificContentSettings* TabSpecificContentSettings::Get(
    int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderViewHost* view = RenderViewHost::FromID(
      render_process_id, render_view_id);
  if (!view)
    return NULL;
  // We loop through the tab contents and compare them with |view|, instead of
  // getting the RVH from each tab contents and comparing its IDs because the
  // latter will miss provisional RenderViewHosts.
  for (TabSpecificList::iterator i = g_tab_specific.Get().begin();
       i != g_tab_specific.Get().end(); ++i) {
    if (view->delegate() == (*i)->tab_contents())
      return (*i);    
  }

  return NULL;
}

void TabSpecificContentSettings::CookiesRead(int render_process_id,
                                             int render_view_id,
                                             const GURL& url,
                                             const net::CookieList& cookie_list,
                                             bool blocked_by_policy) {
  TabSpecificContentSettings* settings = Get(render_process_id, render_view_id);
  if (settings)
    settings->OnCookiesRead(url, cookie_list, blocked_by_policy);
}

void TabSpecificContentSettings::CookieChanged(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    bool blocked_by_policy) {
  TabSpecificContentSettings* settings = Get(render_process_id, render_view_id);
  if (settings)
    settings->OnCookieChanged(url, cookie_line, options, blocked_by_policy);
}

void TabSpecificContentSettings::WebDatabaseAccessed(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const string16& name,
    const string16& display_name,
    bool blocked_by_policy) {
  TabSpecificContentSettings* settings = Get(render_process_id, render_view_id);
  if (settings)
    settings->OnWebDatabaseAccessed(url, name, display_name, blocked_by_policy);
}

void TabSpecificContentSettings::DOMStorageAccessed(int render_process_id,
                                                    int render_view_id,
                                                    const GURL& url,
                                                    DOMStorageType storage_type,
                                                    bool blocked_by_policy) {
  TabSpecificContentSettings* settings = Get(render_process_id, render_view_id);
  if (settings)
    settings->OnLocalStorageAccessed(url, storage_type, blocked_by_policy);
}

void TabSpecificContentSettings::IndexedDBAccessed(int render_process_id,
                                                   int render_view_id,
                                                   const GURL& url,
                                                   const string16& description,
                                                   bool blocked_by_policy) {
  TabSpecificContentSettings* settings = Get(render_process_id, render_view_id);
  if (settings)
    settings->OnIndexedDBAccessed(url, description, blocked_by_policy);
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
    // TODO: it would be nice to have a way of mocking this in tests.
    NotificationService::current()->Notify(
        NotificationType::TAB_CONTENT_SETTINGS_CHANGED,
        Source<TabContents>(tab_contents()),
        NotificationService::NoDetails());
  }
}

void TabSpecificContentSettings::OnContentAccessed(ContentSettingsType type) {
  DCHECK(type != CONTENT_SETTINGS_TYPE_GEOLOCATION)
      << "Geolocation settings handled by OnGeolocationPermissionSet";
  if (!content_accessed_[type]) {
    content_accessed_[type] = true;
    NotificationService::current()->Notify(
        NotificationType::TAB_CONTENT_SETTINGS_CHANGED,
        Source<TabContents>(tab_contents()),
        NotificationService::NoDetails());
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
    container.cookies()->SetCookieWithDetails(url,
                                              cookie->Name(),
                                              cookie->Value(),
                                              cookie->Domain(),
                                              cookie->Path(),
                                              cookie->ExpiryDate(),
                                              cookie->IsSecure(),
                                              cookie->IsHttpOnly());
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
    blocked_local_shared_objects_.cookies()->SetCookieWithOptions(
        url, cookie_line, options);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
  } else {
    allowed_local_shared_objects_.cookies()->SetCookieWithOptions(
        url, cookie_line, options);
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
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENT_SETTINGS_CHANGED,
      Source<TabContents>(tab_contents()),
      NotificationService::NoDetails());
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
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENT_SETTINGS_CHANGED,
      Source<TabContents>(tab_contents()),
      NotificationService::NoDetails());
}

void TabSpecificContentSettings::ClearCookieSpecificContentSettings() {
  blocked_local_shared_objects_.Reset();
  allowed_local_shared_objects_.Reset();
  content_blocked_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  content_accessed_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  content_blockage_indicated_to_user_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENT_SETTINGS_CHANGED,
      Source<TabContents>(tab_contents()),
      NotificationService::NoDetails());
}

void TabSpecificContentSettings::SetPopupsBlocked(bool blocked) {
  content_blocked_[CONTENT_SETTINGS_TYPE_POPUPS] = blocked;
  content_blockage_indicated_to_user_[CONTENT_SETTINGS_TYPE_POPUPS] = false;
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENT_SETTINGS_CHANGED,
      Source<TabContents>(tab_contents()),
      NotificationService::NoDetails());
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

bool TabSpecificContentSettings::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TabSpecificContentSettings, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ContentBlocked, OnContentBlocked)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AppCacheAccessed, OnAppCacheAccessed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TabSpecificContentSettings::DidNavigateMainFramePostCommit(
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (!details.is_in_page) {
    // Clear "blocked" flags.
    ClearBlockedContentSettingsExceptForCookies();
    GeolocationDidNavigate(details);
  }
}

void TabSpecificContentSettings::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    RenderViewHost* render_view_host) {
  if (!is_main_frame)
    return;

  // If we're displaying a network error page do not reset the content
  // settings delegate's cookies so the user has a chance to modify cookie
  // settings.
  if (!is_error_page)
    ClearCookieSpecificContentSettings();
  ClearGeolocationContentSettings();

  HostContentSettingsMap* map =
      tab_contents()->profile()->GetHostContentSettingsMap();
  render_view_host->Send(new ViewMsg_SetContentSettingsForLoadingURL(
      render_view_host->routing_id(), validated_url,
      map->GetContentSettings(validated_url)));
}

void TabSpecificContentSettings::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  DCHECK(type.value == NotificationType::CONTENT_SETTINGS_CHANGED);

  Details<const ContentSettingsDetails> settings_details(details);
  const NavigationController& controller = tab_contents()->controller();
  NavigationEntry* entry = controller.GetActiveEntry();
  GURL entry_url;
  if (entry)
    entry_url = entry->url();
  if (settings_details.ptr()->update_all() ||
      settings_details.ptr()->pattern().Matches(entry_url)) {
    Send(new ViewMsg_SetContentSettingsForCurrentURL(
        entry_url, tab_contents()->profile()->GetHostContentSettingsMap()->
            GetContentSettings(entry_url)));
  }
}

TabSpecificContentSettings::LocalSharedObjectsContainer::
    LocalSharedObjectsContainer(Profile* profile)
    : cookies_(new net::CookieMonster(NULL, NULL)),
      appcaches_(new CannedBrowsingDataAppCacheHelper(profile)),
      databases_(new CannedBrowsingDataDatabaseHelper(profile)),
      indexed_dbs_(new CannedBrowsingDataIndexedDBHelper(profile)),
      local_storages_(new CannedBrowsingDataLocalStorageHelper(profile)),
      session_storages_(new CannedBrowsingDataLocalStorageHelper(profile)) {
  cookies_->SetCookieableSchemes(
      net::CookieMonster::kDefaultCookieableSchemes,
      net::CookieMonster::kDefaultCookieableSchemesCount);
  cookies_->SetKeepExpiredCookies();
}

TabSpecificContentSettings::LocalSharedObjectsContainer::
    ~LocalSharedObjectsContainer() {
}

void TabSpecificContentSettings::LocalSharedObjectsContainer::Reset() {
  cookies_ = new net::CookieMonster(NULL, NULL);
  cookies_->SetCookieableSchemes(
      net::CookieMonster::kDefaultCookieableSchemes,
      net::CookieMonster::kDefaultCookieableSchemesCount);
  cookies_->SetKeepExpiredCookies();
  appcaches_->Reset();
  databases_->Reset();
  indexed_dbs_->Reset();
  local_storages_->Reset();
  session_storages_->Reset();
}

CookiesTreeModel*
TabSpecificContentSettings::LocalSharedObjectsContainer::GetCookiesTreeModel() {
  return new CookiesTreeModel(cookies_,
                              databases_->Clone(),
                              local_storages_->Clone(),
                              session_storages_->Clone(),
                              appcaches_->Clone(),
                              indexed_dbs_->Clone(),
                              true);
}
