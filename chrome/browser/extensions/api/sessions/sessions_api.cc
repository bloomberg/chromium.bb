// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sessions/sessions_api.h"

#include <vector>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/sessions/session_id.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/open_tabs_ui_delegate.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/error_utils.h"
#include "net/base/net_util.h"
#include "ui/base/layout.h"

namespace extensions {

namespace GetRecentlyClosed = api::sessions::GetRecentlyClosed;
namespace GetDevices = api::sessions::GetDevices;
namespace Restore = api::sessions::Restore;
namespace tabs = api::tabs;
namespace windows = api::windows;

const char kNoRecentlyClosedSessionsError[] =
    "There are no recently closed sessions.";
const char kInvalidSessionIdError[] = "Invalid session id: \"*\".";
const char kNoBrowserToRestoreSession[] =
    "There are no browser windows to restore the session.";
const char kSessionSyncError[] = "Synced sessions are not available.";
const char kRestoreInIncognitoError[] =
    "Can not restore sessions in incognito mode.";

// Comparator function for use with std::sort that will sort sessions by
// descending modified_time (i.e., most recent first).
bool SortSessionsByRecency(const browser_sync::SyncedSession* s1,
                           const browser_sync::SyncedSession* s2) {
  return s1->modified_time > s2->modified_time;
}

// Comparator function for use with std::sort that will sort tabs in a window
// by descending timestamp (i.e., most recent first).
bool SortTabsByRecency(const SessionTab* t1, const SessionTab* t2) {
  return t1->timestamp > t2->timestamp;
}

scoped_ptr<tabs::Tab> CreateTabModelHelper(
    Profile* profile,
    const sessions::SerializedNavigationEntry& current_navigation,
    const std::string& session_id,
    int index,
    bool pinned,
    int selected_index,
    const Extension* extension) {
  scoped_ptr<tabs::Tab> tab_struct(new tabs::Tab);

  GURL gurl = current_navigation.virtual_url();
  std::string title = base::UTF16ToUTF8(current_navigation.title());

  tab_struct->session_id.reset(new std::string(session_id));
  tab_struct->url.reset(new std::string(gurl.spec()));
  if (!title.empty()) {
    tab_struct->title.reset(new std::string(title));
  } else {
    const std::string languages =
        profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
    tab_struct->title.reset(new std::string(base::UTF16ToUTF8(
        net::FormatUrl(gurl, languages))));
  }
  tab_struct->index = index;
  tab_struct->pinned = pinned;
  tab_struct->selected = index == selected_index;
  tab_struct->active = false;
  tab_struct->highlighted = false;
  tab_struct->incognito = false;
  ExtensionTabUtil::ScrubTabForExtension(extension, tab_struct.get());
  return tab_struct.Pass();
}

scoped_ptr<windows::Window> CreateWindowModelHelper(
    scoped_ptr<std::vector<linked_ptr<tabs::Tab> > > tabs,
    const std::string& session_id,
    const windows::Window::Type& type,
    const windows::Window::State& state) {
  scoped_ptr<windows::Window> window_struct(new windows::Window);
  window_struct->tabs = tabs.Pass();
  window_struct->session_id.reset(new std::string(session_id));
  window_struct->incognito = false;
  window_struct->always_on_top = false;
  window_struct->focused = false;
  window_struct->type = type;
  window_struct->state = state;
  return window_struct.Pass();
}

scoped_ptr<api::sessions::Session> CreateSessionModelHelper(
    int last_modified,
    scoped_ptr<tabs::Tab> tab,
    scoped_ptr<windows::Window> window) {
  scoped_ptr<api::sessions::Session> session_struct(new api::sessions::Session);
  session_struct->last_modified = last_modified;
  if (tab)
    session_struct->tab = tab.Pass();
  else if (window)
    session_struct->window = window.Pass();
  else
    NOTREACHED();
  return session_struct.Pass();
}

bool is_tab_entry(const TabRestoreService::Entry* entry) {
  return entry->type == TabRestoreService::TAB;
}

bool is_window_entry(const TabRestoreService::Entry* entry) {
  return entry->type == TabRestoreService::WINDOW;
}

scoped_ptr<tabs::Tab> SessionsGetRecentlyClosedFunction::CreateTabModel(
    const TabRestoreService::Tab& tab, int session_id, int selected_index) {
  return CreateTabModelHelper(GetProfile(),
                              tab.navigations[tab.current_navigation_index],
                              base::IntToString(session_id),
                              tab.tabstrip_index,
                              tab.pinned,
                              selected_index,
                              extension());
}

scoped_ptr<windows::Window>
    SessionsGetRecentlyClosedFunction::CreateWindowModel(
        const TabRestoreService::Window& window,
        int session_id) {
  DCHECK(!window.tabs.empty());

  scoped_ptr<std::vector<linked_ptr<tabs::Tab> > > tabs(
      new std::vector<linked_ptr<tabs::Tab> >);
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    tabs->push_back(make_linked_ptr(
        CreateTabModel(window.tabs[i], window.tabs[i].id,
                       window.selected_tab_index).release()));
  }

  return CreateWindowModelHelper(tabs.Pass(),
                                 base::IntToString(session_id),
                                 windows::Window::TYPE_NORMAL,
                                 windows::Window::STATE_NORMAL);
}

scoped_ptr<api::sessions::Session>
    SessionsGetRecentlyClosedFunction::CreateSessionModel(
        const TabRestoreService::Entry* entry) {
  scoped_ptr<tabs::Tab> tab;
  scoped_ptr<windows::Window> window;
  switch (entry->type) {
    case TabRestoreService::TAB:
      tab = CreateTabModel(
          *static_cast<const TabRestoreService::Tab*>(entry), entry->id, -1);
      break;
    case TabRestoreService::WINDOW:
      window = CreateWindowModel(
        *static_cast<const TabRestoreService::Window*>(entry), entry->id);
      break;
    default:
      NOTREACHED();
  }
  return CreateSessionModelHelper(entry->timestamp.ToTimeT(),
                                  tab.Pass(),
                                  window.Pass());
}

bool SessionsGetRecentlyClosedFunction::RunSync() {
  scoped_ptr<GetRecentlyClosed::Params> params(
      GetRecentlyClosed::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  int max_results = api::sessions::MAX_SESSION_RESULTS;
  if (params->filter && params->filter->max_results)
    max_results = *params->filter->max_results;
  EXTENSION_FUNCTION_VALIDATE(max_results >= 0 &&
      max_results <= api::sessions::MAX_SESSION_RESULTS);

  std::vector<linked_ptr<api::sessions::Session> > result;
  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(GetProfile());

  // TabRestoreServiceFactory::GetForProfile() can return NULL (i.e., when in
  // incognito mode)
  if (!tab_restore_service) {
    DCHECK_NE(GetProfile(), GetProfile()->GetOriginalProfile())
        << "TabRestoreService expected for normal profiles";
    results_ = GetRecentlyClosed::Results::Create(
        std::vector<linked_ptr<api::sessions::Session> >());
    return true;
  }

  // List of entries. They are ordered from most to least recent.
  // We prune the list to contain max 25 entries at any time and removes
  // uninteresting entries.
  TabRestoreService::Entries entries = tab_restore_service->entries();
  for (TabRestoreService::Entries::const_iterator it = entries.begin();
       it != entries.end() && static_cast<int>(result.size()) < max_results;
       ++it) {
    TabRestoreService::Entry* entry = *it;
    result.push_back(make_linked_ptr(CreateSessionModel(entry).release()));
  }

  results_ = GetRecentlyClosed::Results::Create(result);
  return true;
}

scoped_ptr<tabs::Tab> SessionsGetDevicesFunction::CreateTabModel(
    const std::string& session_tag,
    const SessionTab& tab,
    int tab_index,
    int selected_index) {
  std::string session_id = SessionId(session_tag, tab.tab_id.id()).ToString();
  return CreateTabModelHelper(
      GetProfile(),
      tab.navigations[tab.normalized_navigation_index()],
      session_id,
      tab_index,
      tab.pinned,
      selected_index,
      extension());
}

scoped_ptr<windows::Window> SessionsGetDevicesFunction::CreateWindowModel(
        const SessionWindow& window, const std::string& session_tag) {
  DCHECK(!window.tabs.empty());

  // Prune tabs that are not syncable or are NewTabPage. Then, sort the tabs
  // from most recent to least recent.
  std::vector<const SessionTab*> tabs_in_window;
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    const SessionTab* tab = window.tabs[i];
    if (tab->navigations.empty())
      continue;
    const sessions::SerializedNavigationEntry& current_navigation =
        tab->navigations.at(tab->normalized_navigation_index());
    if (chrome::IsNTPURL(current_navigation.virtual_url(), GetProfile())) {
      continue;
    }
    tabs_in_window.push_back(tab);
  }
  if (tabs_in_window.empty())
    return scoped_ptr<windows::Window>();
  std::sort(tabs_in_window.begin(), tabs_in_window.end(), SortTabsByRecency);

  scoped_ptr<std::vector<linked_ptr<tabs::Tab> > > tabs(
      new std::vector<linked_ptr<tabs::Tab> >);
  for (size_t i = 0; i < tabs_in_window.size(); ++i) {
    tabs->push_back(make_linked_ptr(
        CreateTabModel(session_tag, *tabs_in_window[i], i,
                       window.selected_tab_index).release()));
  }

  std::string session_id =
      SessionId(session_tag, window.window_id.id()).ToString();

  windows::Window::Type type = windows::Window::TYPE_NONE;
  switch (window.type) {
    case Browser::TYPE_TABBED:
      type = windows::Window::TYPE_NORMAL;
      break;
    case Browser::TYPE_POPUP:
      type = windows::Window::TYPE_POPUP;
      break;
  }

  windows::Window::State state = windows::Window::STATE_NONE;
  switch (window.show_state) {
    case ui::SHOW_STATE_NORMAL:
      state = windows::Window::STATE_NORMAL;
      break;
    case ui::SHOW_STATE_MINIMIZED:
      state = windows::Window::STATE_MINIMIZED;
      break;
    case ui::SHOW_STATE_MAXIMIZED:
      state = windows::Window::STATE_MAXIMIZED;
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      state = windows::Window::STATE_FULLSCREEN;
      break;
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_INACTIVE:
    case ui::SHOW_STATE_DETACHED:
    case ui::SHOW_STATE_END:
      break;
  }

  scoped_ptr<windows::Window> window_struct(
      CreateWindowModelHelper(tabs.Pass(), session_id, type, state));
  // TODO(dwankri): Dig deeper to resolve bounds not being optional, so closed
  // windows in GetRecentlyClosed can have set values in Window helper.
  window_struct->left.reset(new int(window.bounds.x()));
  window_struct->top.reset(new int(window.bounds.y()));
  window_struct->width.reset(new int(window.bounds.width()));
  window_struct->height.reset(new int(window.bounds.height()));

  return window_struct.Pass();
}

scoped_ptr<api::sessions::Session>
SessionsGetDevicesFunction::CreateSessionModel(
    const SessionWindow& window, const std::string& session_tag) {
  scoped_ptr<windows::Window> window_model(
      CreateWindowModel(window, session_tag));
  // There is a chance that after pruning uninteresting tabs the window will be
  // empty.
  return !window_model ? scoped_ptr<api::sessions::Session>()
      : CreateSessionModelHelper(window.timestamp.ToTimeT(),
                                 scoped_ptr<tabs::Tab>(),
                                 window_model.Pass());
}

scoped_ptr<api::sessions::Device> SessionsGetDevicesFunction::CreateDeviceModel(
    const browser_sync::SyncedSession* session) {
  int max_results = api::sessions::MAX_SESSION_RESULTS;
  // Already validated in RunAsync().
  scoped_ptr<GetDevices::Params> params(GetDevices::Params::Create(*args_));
  if (params->filter && params->filter->max_results)
    max_results = *params->filter->max_results;

  scoped_ptr<api::sessions::Device> device_struct(new api::sessions::Device);
  device_struct->info = session->session_name;
  device_struct->device_name = session->session_name;

  for (browser_sync::SyncedSession::SyncedWindowMap::const_iterator it =
       session->windows.begin(); it != session->windows.end() &&
       static_cast<int>(device_struct->sessions.size()) < max_results; ++it) {
    scoped_ptr<api::sessions::Session> session_model(CreateSessionModel(
        *it->second, session->session_tag));
    if (session_model)
      device_struct->sessions.push_back(make_linked_ptr(
          session_model.release()));
  }
  return device_struct.Pass();
}

bool SessionsGetDevicesFunction::RunSync() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(GetProfile());
  if (!(service && service->GetPreferredDataTypes().Has(syncer::SESSIONS))) {
    // Sync not enabled.
    results_ = GetDevices::Results::Create(
        std::vector<linked_ptr<api::sessions::Device> >());
    return true;
  }

  browser_sync::OpenTabsUIDelegate* open_tabs =
      service->GetOpenTabsUIDelegate();
  std::vector<const browser_sync::SyncedSession*> sessions;
  if (!(open_tabs && open_tabs->GetAllForeignSessions(&sessions))) {
    results_ = GetDevices::Results::Create(
        std::vector<linked_ptr<api::sessions::Device> >());
    return true;
  }

  scoped_ptr<GetDevices::Params> params(GetDevices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  if (params->filter && params->filter->max_results) {
    EXTENSION_FUNCTION_VALIDATE(*params->filter->max_results >= 0 &&
        *params->filter->max_results <= api::sessions::MAX_SESSION_RESULTS);
  }

  std::vector<linked_ptr<api::sessions::Device> > result;
  // Sort sessions from most recent to least recent.
  std::sort(sessions.begin(), sessions.end(), SortSessionsByRecency);
  for (size_t i = 0; i < sessions.size(); ++i) {
    result.push_back(make_linked_ptr(CreateDeviceModel(sessions[i]).release()));
  }

  results_ = GetDevices::Results::Create(result);
  return true;
}

void SessionsRestoreFunction::SetInvalidIdError(const std::string& invalid_id) {
  SetError(ErrorUtils::FormatErrorMessage(kInvalidSessionIdError, invalid_id));
}


void SessionsRestoreFunction::SetResultRestoredTab(
    content::WebContents* contents) {
  scoped_ptr<base::DictionaryValue> tab_value(
      ExtensionTabUtil::CreateTabValue(contents, extension()));
  scoped_ptr<tabs::Tab> tab(tabs::Tab::FromValue(*tab_value));
  scoped_ptr<api::sessions::Session> restored_session(CreateSessionModelHelper(
      base::Time::Now().ToTimeT(),
      tab.Pass(),
      scoped_ptr<windows::Window>()));
  results_ = Restore::Results::Create(*restored_session);
}

bool SessionsRestoreFunction::SetResultRestoredWindow(int window_id) {
  WindowController* controller = NULL;
  if (!windows_util::GetWindowFromWindowID(this, window_id, &controller)) {
    // error_ is set by GetWindowFromWindowId function call.
    return false;
  }
  scoped_ptr<base::DictionaryValue> window_value(
      controller->CreateWindowValueWithTabs(extension()));
  scoped_ptr<windows::Window> window(windows::Window::FromValue(
      *window_value));
  results_ = Restore::Results::Create(*CreateSessionModelHelper(
      base::Time::Now().ToTimeT(),
      scoped_ptr<tabs::Tab>(),
      window.Pass()));
  return true;
}

bool SessionsRestoreFunction::RestoreMostRecentlyClosed(Browser* browser) {
  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(GetProfile());
  chrome::HostDesktopType host_desktop_type = browser->host_desktop_type();
  TabRestoreService::Entries entries = tab_restore_service->entries();

  if (entries.empty()) {
    SetError(kNoRecentlyClosedSessionsError);
    return false;
  }

  bool is_window = is_window_entry(entries.front());
  TabRestoreServiceDelegate* delegate =
      TabRestoreServiceDelegate::FindDelegateForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  std::vector<content::WebContents*> contents =
      tab_restore_service->RestoreMostRecentEntry(delegate, host_desktop_type);
  DCHECK(contents.size());

  if (is_window) {
    return SetResultRestoredWindow(
        ExtensionTabUtil::GetWindowIdOfTab(contents[0]));
  }

  SetResultRestoredTab(contents[0]);
  return true;
}

bool SessionsRestoreFunction::RestoreLocalSession(const SessionId& session_id,
                                                  Browser* browser) {
  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(GetProfile());
  chrome::HostDesktopType host_desktop_type = browser->host_desktop_type();
  TabRestoreService::Entries entries = tab_restore_service->entries();

  if (entries.empty()) {
    SetInvalidIdError(session_id.ToString());
    return false;
  }

  // Check if the recently closed list contains an entry with the provided id.
  bool is_window = false;
  for (TabRestoreService::Entries::iterator it = entries.begin();
       it != entries.end(); ++it) {
    if ((*it)->id == session_id.id()) {
      // The only time a full window is being restored is if the entry ID
      // matches the provided ID and the entry type is Window.
      is_window = is_window_entry(*it);
      break;
    }
  }

  TabRestoreServiceDelegate* delegate =
      TabRestoreServiceDelegate::FindDelegateForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  std::vector<content::WebContents*> contents =
      tab_restore_service->RestoreEntryById(delegate,
                                            session_id.id(),
                                            host_desktop_type,
                                            UNKNOWN);
  // If the ID is invalid, contents will be empty.
  if (!contents.size()) {
    SetInvalidIdError(session_id.ToString());
    return false;
  }

  // Retrieve the window through any of the tabs in contents.
  if (is_window) {
    return SetResultRestoredWindow(
        ExtensionTabUtil::GetWindowIdOfTab(contents[0]));
  }

  SetResultRestoredTab(contents[0]);
  return true;
}

bool SessionsRestoreFunction::RestoreForeignSession(const SessionId& session_id,
                                                    Browser* browser) {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(GetProfile());
  if (!(service && service->GetPreferredDataTypes().Has(syncer::SESSIONS))) {
    SetError(kSessionSyncError);
    return false;
  }
  browser_sync::OpenTabsUIDelegate* open_tabs =
      service->GetOpenTabsUIDelegate();
  if (!open_tabs) {
    SetError(kSessionSyncError);
    return false;
  }

  const SessionTab* tab = NULL;
  if (open_tabs->GetForeignTab(session_id.session_tag(),
                               session_id.id(),
                               &tab)) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    content::WebContents* contents = tab_strip->GetActiveWebContents();

    content::WebContents* tab_contents =
        SessionRestore::RestoreForeignSessionTab(contents, *tab,
                                                 NEW_FOREGROUND_TAB);
    SetResultRestoredTab(tab_contents);
    return true;
  }

  // Restoring a full window.
  std::vector<const SessionWindow*> windows;
  if (!open_tabs->GetForeignSession(session_id.session_tag(), &windows)) {
    SetInvalidIdError(session_id.ToString());
    return false;
  }

  std::vector<const SessionWindow*>::const_iterator window = windows.begin();
  while (window != windows.end()
         && (*window)->window_id.id() != session_id.id()) {
    ++window;
  }
  if (window == windows.end()) {
    SetInvalidIdError(session_id.ToString());
    return false;
  }

  chrome::HostDesktopType host_desktop_type = browser->host_desktop_type();
  // Only restore one window at a time.
  std::vector<Browser*> browsers = SessionRestore::RestoreForeignSessionWindows(
      GetProfile(), host_desktop_type, window, window + 1);
  // Will always create one browser because we only restore one window per call.
  DCHECK_EQ(1u, browsers.size());
  return SetResultRestoredWindow(ExtensionTabUtil::GetWindowId(browsers[0]));
}

bool SessionsRestoreFunction::RunSync() {
  scoped_ptr<Restore::Params> params(Restore::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Browser* browser = chrome::FindBrowserWithProfile(
      GetProfile(), chrome::HOST_DESKTOP_TYPE_NATIVE);
  if (!browser) {
    SetError(kNoBrowserToRestoreSession);
    return false;
  }

  if (GetProfile() != GetProfile()->GetOriginalProfile()) {
    SetError(kRestoreInIncognitoError);
    return false;
  }

  if (!params->session_id)
    return RestoreMostRecentlyClosed(browser);

  scoped_ptr<SessionId> session_id(SessionId::Parse(*params->session_id));
  if (!session_id) {
    SetInvalidIdError(*params->session_id);
    return false;
  }

  return session_id->IsForeign() ?
      RestoreForeignSession(*session_id, browser)
      : RestoreLocalSession(*session_id, browser);
}

SessionsEventRouter::SessionsEventRouter(Profile* profile)
    : profile_(profile),
      tab_restore_service_(TabRestoreServiceFactory::GetForProfile(profile)) {
  // TabRestoreServiceFactory::GetForProfile() can return NULL (i.e., when in
  // incognito mode)
  if (tab_restore_service_) {
    tab_restore_service_->LoadTabsFromLastSession();
    tab_restore_service_->AddObserver(this);
  }
}

SessionsEventRouter::~SessionsEventRouter() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void SessionsEventRouter::TabRestoreServiceChanged(
    TabRestoreService* service) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  EventRouter::Get(profile_)->BroadcastEvent(make_scoped_ptr(
      new Event(api::sessions::OnChanged::kEventName, args.Pass())));
}

void SessionsEventRouter::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

SessionsAPI::SessionsAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter::Get(browser_context_)->RegisterObserver(this,
      api::sessions::OnChanged::kEventName);
}

SessionsAPI::~SessionsAPI() {
}

void SessionsAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<SessionsAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

BrowserContextKeyedAPIFactory<SessionsAPI>*
SessionsAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void SessionsAPI::OnListenerAdded(const EventListenerInfo& details) {
  sessions_event_router_.reset(
      new SessionsEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions
