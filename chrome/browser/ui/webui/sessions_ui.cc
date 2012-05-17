// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sessions_ui.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

ChromeWebUIDataSource* CreateSessionsUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUISessionsHost);

  source->AddLocalizedString("sessionsTitle", IDS_SESSIONS_TITLE);
  source->AddLocalizedString("sessionsCountFormat",
                             IDS_SESSIONS_SESSION_COUNT_BANNER_FORMAT);
  source->AddLocalizedString("noSessionsMessage",
                             IDS_SESSIONS_NO_SESSIONS_MESSAGE);
  source->AddLocalizedString("magicCountFormat",
                             IDS_SESSIONS_MAGIC_LIST_BANNER_FORMAT);
  source->AddLocalizedString("noMagicMessage",
                             IDS_SESSIONS_NO_MAGIC_MESSAGE);

  source->set_json_path("strings.js");
  source->add_resource_path("sessions.js", IDR_SESSIONS_JS);
  source->set_default_resource(IDR_SESSIONS_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// SessionsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome://sessions/ page.
class SessionsDOMHandler : public WebUIMessageHandler {
 public:
  SessionsDOMHandler();
  virtual ~SessionsDOMHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Asynchronously fetches the session list. Called from JS.
  void HandleRequestSessions(const ListValue* args);

  // Sends the recent session list to JS.
  void UpdateUI();

  // Returns the model associated for getting the list of sessions from sync.
  browser_sync::SessionModelAssociator* GetModelAssociator();

  // Appends each entry in |tabs| to |tab_list| as a DictonaryValue.
  void GetTabList(const std::vector<SessionTab*>& tabs, ListValue* tab_list);

  // Appends each entry in |windows| to |window_list| as a DictonaryValue.
  void GetWindowList(
      const browser_sync::SyncedSession::SyncedWindowMap& windows,
      ListValue* window_list);

  // Appends each entry in |sessions| to |session_list| as a DictonaryValue.
  void GetSessionList(
      const std::vector<const browser_sync::SyncedSession*>& sessions,
      ListValue* session_list);

  // Traverses all tabs in |sessions| and adds them to |all_tabs|.
  void GetAllTabs(
      const std::vector<const browser_sync::SyncedSession*>& sessions,
      std::vector<SessionTab*>* all_tabs);

  // Creates a "magic" list of tabs from all the sessions.
  void CreateMagicTabList(
      const std::vector<const browser_sync::SyncedSession*>& sessions,
      ListValue* tab_list);

  DISALLOW_COPY_AND_ASSIGN(SessionsDOMHandler);
};

SessionsDOMHandler::SessionsDOMHandler() {
}

SessionsDOMHandler::~SessionsDOMHandler() {
}

void SessionsDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestSessionList",
      base::Bind(&SessionsDOMHandler::HandleRequestSessions,
                 base::Unretained(this)));
}

void SessionsDOMHandler::HandleRequestSessions(const ListValue* args) {
  UpdateUI();
}

browser_sync::SessionModelAssociator* SessionsDOMHandler::GetModelAssociator() {
  // We only want to get the model associator if there is one, and it is done
  // syncing sessions.
  Profile* profile = Profile::FromWebUI(web_ui());
  ProfileSyncServiceFactory* f = ProfileSyncServiceFactory::GetInstance();
  if (!f->HasProfileSyncService(profile))
    return NULL;
  ProfileSyncService* service = f->GetForProfile(profile);
  if (!service->ShouldPushChanges())
    return NULL;
  return service->GetSessionModelAssociator();
}

void SessionsDOMHandler::GetTabList(
    const std::vector<SessionTab*>& tabs, ListValue* tab_list) {
  for (std::vector<SessionTab*>::const_iterator it = tabs.begin();
       it != tabs.end(); ++it) {
    const SessionTab* tab = *it;
    const int nav_index = tab->current_navigation_index;
    // Range-check |nav_index| as it has been observed to be invalid sometimes.
    if (nav_index < 0 ||
        static_cast<size_t>(nav_index) >= tab->navigations.size()) {
      continue;
    }
    const TabNavigation& nav = tab->navigations[nav_index];
    scoped_ptr<DictionaryValue> tab_data(new DictionaryValue());
    tab_data->SetInteger("id", tab->tab_id.id());
    tab_data->SetDouble("timestamp",
        static_cast<double> (tab->timestamp.ToInternalValue()));
    if (nav.title().empty())
      tab_data->SetString("title", nav.virtual_url().spec());
    else
      tab_data->SetString("title", nav.title());
    tab_data->SetString("url", nav.virtual_url().spec());
    tab_list->Append(tab_data.release());
  }
}

void SessionsDOMHandler::GetWindowList(
    const browser_sync::SyncedSession::SyncedWindowMap& windows,
    ListValue* window_list) {
  for (browser_sync::SyncedSession::SyncedWindowMap::const_iterator it =
      windows.begin(); it != windows.end(); ++it) {
    const SessionWindow* window = it->second;
    scoped_ptr<DictionaryValue> window_data(new DictionaryValue());
    window_data->SetInteger("id", window->window_id.id());
    window_data->SetDouble("timestamp",
        static_cast<double> (window->timestamp.ToInternalValue()));
    scoped_ptr<ListValue> tab_list(new ListValue());
    GetTabList(window->tabs, tab_list.get());
    window_data->Set("tabs", tab_list.release());
    window_list->Append(window_data.release());
  }
}

void SessionsDOMHandler::GetSessionList(
    const std::vector<const browser_sync::SyncedSession*>& sessions,
    ListValue* session_list) {
  for (std::vector<const browser_sync::SyncedSession*>::const_iterator it =
       sessions.begin(); it != sessions.end(); ++it) {
    const browser_sync::SyncedSession* session = *it;
    scoped_ptr<DictionaryValue> session_data(new DictionaryValue());
    session_data->SetString("tag", session->session_tag);
    session_data->SetString("name", session->session_name);
    scoped_ptr<ListValue> window_list(new ListValue());
    GetWindowList(session->windows, window_list.get());
    session_data->Set("windows", window_list.release());
    session_list->Append(session_data.release());
  }
}

void SessionsDOMHandler::GetAllTabs(
    const std::vector<const browser_sync::SyncedSession*>& sessions,
    std::vector<SessionTab*>* all_tabs) {
  for (size_t i = 0; i < sessions.size(); i++) {
    const browser_sync::SyncedSession::SyncedWindowMap& windows =
        sessions[i]->windows;
    for (browser_sync::SyncedSession::SyncedWindowMap::const_iterator iter =
             windows.begin();
         iter != windows.end(); ++iter) {
      const std::vector<SessionTab*>& tabs = iter->second->tabs;
      all_tabs->insert(all_tabs->end(), tabs.begin(), tabs.end());
    }
  }
}

// Comparator function for sort() in CreateMagicTabList() below.
bool CompareTabsByTimestamp(SessionTab* lhs, SessionTab* rhs) {
  return lhs->timestamp.ToInternalValue() > rhs->timestamp.ToInternalValue();
}

void SessionsDOMHandler::CreateMagicTabList(
    const std::vector<const browser_sync::SyncedSession*>& sessions,
    ListValue* tab_list) {
  std::vector<SessionTab*> all_tabs;
  GetAllTabs(sessions, &all_tabs);

  // Sort the list by timestamp - newest first.
  std::sort(all_tabs.begin(), all_tabs.end(), CompareTabsByTimestamp);

  // Truncate the list if necessary.
  const size_t kMagicTabListMaxSize = 10;
  if (all_tabs.size() > kMagicTabListMaxSize)
    all_tabs.resize(kMagicTabListMaxSize);

  GetTabList(all_tabs, tab_list);
}

void SessionsDOMHandler::UpdateUI() {
  ListValue session_list;
  ListValue magic_list;

  browser_sync::SessionModelAssociator* associator = GetModelAssociator();
  // Make sure the associator has been created.
  if (associator) {
    std::vector<const browser_sync::SyncedSession*> sessions;
    if (associator->GetAllForeignSessions(&sessions)) {
      GetSessionList(sessions, &session_list);
      CreateMagicTabList(sessions, &magic_list);
    }
  }

  // Send the results to JavaScript, even if the lists are empty, so that the
  // UI can show a message that there is nothing.
  web_ui()->CallJavascriptFunction("updateSessionList",
                                   session_list,
                                   magic_list);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// SessionsUI
//
///////////////////////////////////////////////////////////////////////////////

SessionsUI::SessionsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new SessionsDOMHandler());

  // Set up the chrome://sessions/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, CreateSessionsUIHTMLSource());
}

// static
base::RefCountedMemory* SessionsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_HISTORY_FAVICON,
                            ui::SCALE_FACTOR_100P);
}
