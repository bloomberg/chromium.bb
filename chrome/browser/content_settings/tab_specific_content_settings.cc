// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/tab_specific_content_settings.h"

#include <list>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/common/view_messages.h"
#include "webkit/fileapi/file_system_types.h"

using content::BrowserThread;

namespace {
typedef std::list<TabSpecificContentSettings*> TabSpecificList;
static base::LazyInstance<TabSpecificList> g_tab_specific =
    LAZY_INSTANCE_INITIALIZER;
}

bool TabSpecificContentSettings::LocalSharedObjectsContainer::empty() const {
  return appcaches_->empty() &&
      cookies_->empty() &&
      databases_->empty() &&
      file_systems_->empty() &&
      indexed_dbs_->empty() &&
      local_storages_->empty() &&
      session_storages_->empty();
}

TabSpecificContentSettings::TabSpecificContentSettings(TabContents* tab)
    : TabContentsObserver(tab),
      profile_(Profile::FromBrowserContext(tab->browser_context())),
      allowed_local_shared_objects_(profile_),
      blocked_local_shared_objects_(profile_),
      geolocation_settings_state_(profile_),
      load_plugins_link_enabled_(true) {
  ClearBlockedContentSettingsExceptForCookies();
  ClearCookieSpecificContentSettings();
  g_tab_specific.Get().push_back(this);

  registrar_.Add(this, chrome::NOTIFICATION_CONTENT_SETTINGS_CHANGED,
                 content::Source<HostContentSettingsMap>(
                     profile_->GetHostContentSettingsMap()));
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

// static
void TabSpecificContentSettings::CookiesRead(int render_process_id,
                                             int render_view_id,
                                             const GURL& url,
                                             const net::CookieList& cookie_list,
                                             bool blocked_by_policy) {
  TabSpecificContentSettings* settings = Get(render_process_id, render_view_id);
  if (settings)
    settings->OnCookiesRead(url, cookie_list, blocked_by_policy);
}

// static
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

// static
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

// static
void TabSpecificContentSettings::DOMStorageAccessed(int render_process_id,
                                                    int render_view_id,
                                                    const GURL& url,
                                                    DOMStorageType storage_type,
                                                    bool blocked_by_policy) {
  TabSpecificContentSettings* settings = Get(render_process_id, render_view_id);
  if (settings)
    settings->OnLocalStorageAccessed(url, storage_type, blocked_by_policy);
}

// static
void TabSpecificContentSettings::IndexedDBAccessed(int render_process_id,
                                                   int render_view_id,
                                                   const GURL& url,
                                                   const string16& description,
                                                   bool blocked_by_policy) {
  TabSpecificContentSettings* settings = Get(render_process_id, render_view_id);
  if (settings)
    settings->OnIndexedDBAccessed(url, description, blocked_by_policy);
}

// static
void TabSpecificContentSettings::FileSystemAccessed(int render_process_id,
                                                    int render_view_id,
                                                    const GURL& url,
                                                    bool blocked_by_policy) {
  TabSpecificContentSettings* settings = Get(render_process_id, render_view_id);
  if (settings)
    settings->OnFileSystemAccessed(url, blocked_by_policy);
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
    CR_DEFINE_STATIC_LOCAL(std::set<std::string>, empty_set, ());
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
  // Unless UI for resource content settings is enabled, ignore the resource
  // identifier.
  // TODO(bauerb): The UI to unblock content should be disabled if the content
  // setting was not set by the user.
  std::string identifier;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableResourceContentSettings)) {
    identifier = resource_identifier;
  }
  if (!identifier.empty())
    AddBlockedResource(type, identifier);
  if (!content_blocked_[type]) {
    content_blocked_[type] = true;
    // TODO: it would be nice to have a way of mocking this in tests.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_TAB_CONTENT_SETTINGS_CHANGED,
        content::Source<TabContents>(tab_contents()),
        content::NotificationService::NoDetails());
  }
}

void TabSpecificContentSettings::OnContentAccessed(ContentSettingsType type) {
  DCHECK(type != CONTENT_SETTINGS_TYPE_GEOLOCATION)
      << "Geolocation settings handled by OnGeolocationPermissionSet";
  if (!content_accessed_[type]) {
    content_accessed_[type] = true;
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_TAB_CONTENT_SETTINGS_CHANGED,
        content::Source<TabContents>(tab_contents()),
        content::NotificationService::NoDetails());
  }
}

void TabSpecificContentSettings::OnCookiesRead(
    const GURL& url,
    const net::CookieList& cookie_list,
    bool blocked_by_policy) {
  if (cookie_list.empty())
    return;
  if (blocked_by_policy) {
    blocked_local_shared_objects_.cookies()->AddReadCookies(
        url, cookie_list);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
  } else {
    allowed_local_shared_objects_.cookies()->AddReadCookies(
        url, cookie_list);
    OnContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES);
  }
}

void TabSpecificContentSettings::OnCookieChanged(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.cookies()->AddChangedCookie(
        url, cookie_line, options);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
  } else {
    allowed_local_shared_objects_.cookies()->AddChangedCookie(
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
  } else {
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

void TabSpecificContentSettings::OnFileSystemAccessed(
    const GURL& url,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.file_systems()->AddFileSystem(url,
        fileapi::kFileSystemTypeTemporary, 0);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
  } else {
    allowed_local_shared_objects_.file_systems()->AddFileSystem(url,
        fileapi::kFileSystemTypeTemporary, 0);
    OnContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES);
  }
}
void TabSpecificContentSettings::OnGeolocationPermissionSet(
    const GURL& requesting_origin,
    bool allowed) {
  geolocation_settings_state_.OnGeolocationPermissionSet(requesting_origin,
                                                         allowed);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENT_SETTINGS_CHANGED,
      content::Source<TabContents>(tab_contents()),
      content::NotificationService::NoDetails());
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
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENT_SETTINGS_CHANGED,
      content::Source<TabContents>(tab_contents()),
      content::NotificationService::NoDetails());
}

void TabSpecificContentSettings::ClearCookieSpecificContentSettings() {
  blocked_local_shared_objects_.Reset();
  allowed_local_shared_objects_.Reset();
  content_blocked_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  content_accessed_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  content_blockage_indicated_to_user_[CONTENT_SETTINGS_TYPE_COOKIES] = false;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENT_SETTINGS_CHANGED,
      content::Source<TabContents>(tab_contents()),
      content::NotificationService::NoDetails());
}

void TabSpecificContentSettings::SetPopupsBlocked(bool blocked) {
  content_blocked_[CONTENT_SETTINGS_TYPE_POPUPS] = blocked;
  content_blockage_indicated_to_user_[CONTENT_SETTINGS_TYPE_POPUPS] = false;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENT_SETTINGS_CHANGED,
      content::Source<TabContents>(tab_contents()),
      content::NotificationService::NoDetails());
}

void TabSpecificContentSettings::GeolocationDidNavigate(
      const content::LoadCommittedDetails& details) {
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
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ContentBlocked, OnContentBlocked)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TabSpecificContentSettings::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
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
}

void TabSpecificContentSettings::AppCacheAccessed(const GURL& manifest_url,
                                                  bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.appcaches()->AddAppCache(manifest_url);
    OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
  } else {
    allowed_local_shared_objects_.appcaches()->AddAppCache(manifest_url);
    OnContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES);
  }
}

void TabSpecificContentSettings::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_CONTENT_SETTINGS_CHANGED);

  content::Details<const ContentSettingsDetails> settings_details(details);
  const NavigationController& controller = tab_contents()->controller();
  NavigationEntry* entry = controller.GetActiveEntry();
  GURL entry_url;
  if (entry)
    entry_url = entry->url();
  if (settings_details.ptr()->update_all() ||
      // The active NavigationEntry is the URL in the URL field of a tab.
      // Currently this should be matched by the |primary_pattern|.
      settings_details.ptr()->primary_pattern().Matches(entry_url)) {
    Profile* profile =
        Profile::FromBrowserContext(tab_contents()->browser_context());
    RendererContentSettingRules rules;
    GetRendererContentSettingRules(profile->GetHostContentSettingsMap(),
                                   &rules);
    Send(new ChromeViewMsg_SetContentSettingRules(rules));
  }
}

TabSpecificContentSettings::LocalSharedObjectsContainer::
    LocalSharedObjectsContainer(Profile* profile)
    : appcaches_(new CannedBrowsingDataAppCacheHelper(profile)),
      cookies_(new CannedBrowsingDataCookieHelper(profile)),
      databases_(new CannedBrowsingDataDatabaseHelper(profile)),
      file_systems_(new CannedBrowsingDataFileSystemHelper(profile)),
      indexed_dbs_(new CannedBrowsingDataIndexedDBHelper()),
      local_storages_(new CannedBrowsingDataLocalStorageHelper(profile)),
      session_storages_(new CannedBrowsingDataLocalStorageHelper(profile)) {
}

TabSpecificContentSettings::LocalSharedObjectsContainer::
    ~LocalSharedObjectsContainer() {
}

void TabSpecificContentSettings::LocalSharedObjectsContainer::Reset() {
  appcaches_->Reset();
  cookies_->Reset();
  databases_->Reset();
  file_systems_->Reset();
  indexed_dbs_->Reset();
  local_storages_->Reset();
  session_storages_->Reset();
}

CookiesTreeModel*
TabSpecificContentSettings::LocalSharedObjectsContainer::GetCookiesTreeModel() {
  return new CookiesTreeModel(cookies_->Clone(),
                              databases_->Clone(),
                              local_storages_->Clone(),
                              session_storages_->Clone(),
                              appcaches_->Clone(),
                              indexed_dbs_->Clone(),
                              file_systems_->Clone(),
                              NULL,
                              true);
}
