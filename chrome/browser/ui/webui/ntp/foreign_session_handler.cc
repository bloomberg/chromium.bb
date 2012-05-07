// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/foreign_session_handler.h"

#include <algorithm>
#include <string>
#include <vector>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/scoped_vector.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/session_favicon_source.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_ui.h"

namespace browser_sync {

// Maximum number of session we're going to display on the NTP
static const size_t kMaxSessionsToShow = 10;

// Invalid value, used to note that we don't have a tab or window number.
static const int kInvalidId = -1;

namespace {

// Comparator function for use with std::sort that will sort sessions by
// descending modified_time (i.e., most recent first).
bool SortSessionsByRecency(const SyncedSession* s1, const SyncedSession* s2) {
  return s1->modified_time > s2->modified_time;
}

}  // namepace

ForeignSessionHandler::ForeignSessionHandler() {
}

void ForeignSessionHandler::RegisterMessages() {
  Init();
  web_ui()->RegisterMessageCallback("getForeignSessions",
      base::Bind(&ForeignSessionHandler::HandleGetForeignSessions,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openForeignSession",
      base::Bind(&ForeignSessionHandler::HandleOpenForeignSession,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("deleteForeignSession",
      base::Bind(&ForeignSessionHandler::HandleDeleteForeignSession,
                 base::Unretained(this)));
}

void ForeignSessionHandler::Init() {
  // TODO(dubroy): Change this to only observe this notification on the current
  // profile, rather than all sources (crbug.com/124717).
  registrar_.Add(this, chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
                 content::NotificationService::AllSources());

  Profile* profile = Profile::FromWebUI(web_ui());
  registrar_.Add(this, chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_FOREIGN_SESSION_DISABLED,
                 content::Source<Profile>(profile));

  // Add the data source for synced favicons.
  ChromeURLDataManager::AddDataSource(profile,
      new SessionFaviconSource(profile));
}

void ForeignSessionHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  ListValue list_value;
  switch (type) {
    case chrome::NOTIFICATION_SYNC_CONFIGURE_DONE:
    case chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED:
    case chrome::NOTIFICATION_FOREIGN_SESSION_DISABLED:
      HandleGetForeignSessions(&list_value);
      break;
    default:
      NOTREACHED();
  }
}

SessionModelAssociator* ForeignSessionHandler::GetModelAssociator() {
  Profile* profile = Profile::FromWebUI(web_ui());
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  if (service == NULL)
    return NULL;

  // Only return the associator if it exists and it is done syncing sessions.
  SessionModelAssociator* model_associator =
      service->GetSessionModelAssociator();
  if (!service->ShouldPushChanges())
    return NULL;
  return model_associator;
}

bool ForeignSessionHandler::IsTabSyncEnabled() {
  Profile* profile = Profile::FromWebUI(web_ui());
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  return service && service->GetPreferredDataTypes().Has(syncable::SESSIONS);
}

string16 ForeignSessionHandler::FormatSessionTime(const base::Time& time) {
  // Return a time like "1 hour ago", "2 days ago", etc.
  return TimeFormat::TimeElapsed(base::Time::Now() - time);
}

void ForeignSessionHandler::HandleGetForeignSessions(const ListValue* args) {
  SessionModelAssociator* associator = GetModelAssociator();
  std::vector<const SyncedSession*> sessions;

  ListValue session_list;
  if (associator && associator->GetAllForeignSessions(&sessions)) {
    // Sort sessions from most recent to least recent.
    std::sort(sessions.begin(), sessions.end(), SortSessionsByRecency);

    // Note: we don't own the SyncedSessions themselves.
    for (size_t i = 0; i < sessions.size() && i < kMaxSessionsToShow; ++i) {
      const SyncedSession* session = sessions[i];
      scoped_ptr<DictionaryValue> session_data(new DictionaryValue());
      session_data->SetString("tag", session->session_tag);
      session_data->SetString("name", session->session_name);
      session_data->SetString("modifiedTime",
                              FormatSessionTime(session->modified_time));
      scoped_ptr<ListValue> window_list(new ListValue());
      for (SyncedSession::SyncedWindowMap::const_iterator it =
           session->windows.begin(); it != session->windows.end(); ++it) {
        SessionWindow* window = it->second;
        scoped_ptr<DictionaryValue> window_data(new DictionaryValue());
        if (SessionWindowToValue(*window, window_data.get()))
          window_list->Append(window_data.release());
      }

      session_data->Set("windows", window_list.release());
      session_list.Append(session_data.release());
    }
  }
  base::FundamentalValue tab_sync_enabled(IsTabSyncEnabled());
  web_ui()->CallJavascriptFunction("ntp.setForeignSessions",
                                   session_list,
                                   tab_sync_enabled);
}

void ForeignSessionHandler::HandleOpenForeignSession(const ListValue* args) {
  size_t num_args = args->GetSize();
  // Expect either 1 or 8 args. For restoring an entire session, only
  // one argument is required -- the session tag. To restore a tab,
  // the additional args required are the window id, the tab id,
  // and 4 properties of the event object (button, altKey, ctrlKey,
  // metaKey, shiftKey) for determining how to open the tab.
  if (num_args != 8U && num_args != 1U) {
    LOG(ERROR) << "openForeignSession called with " << args->GetSize()
               << " arguments.";
    return;
  }

  // Extract the session tag (always provided).
  std::string session_string_value;
  if (!args->GetString(0, &session_string_value)) {
    LOG(ERROR) << "Failed to extract session tag.";
    return;
  }

  // Extract window number.
  std::string window_num_str;
  int window_num = kInvalidId;
  if (num_args >= 2 && (!args->GetString(1, &window_num_str) ||
      !base::StringToInt(window_num_str, &window_num))) {
    LOG(ERROR) << "Failed to extract window number.";
    return;
  }

  // Extract tab id.
  std::string tab_id_str;
  SessionID::id_type tab_id = kInvalidId;
  if (num_args >= 3 && (!args->GetString(2, &tab_id_str) ||
      !base::StringToInt(tab_id_str, &tab_id))) {
    LOG(ERROR) << "Failed to extract tab SessionID.";
    return;
  }

  SessionModelAssociator* associator = GetModelAssociator();

  Profile* profile = Profile::FromWebUI(web_ui());
  if (tab_id != kInvalidId) {
    // We don't actually care about |window_num|, this is just a sanity check.
    DCHECK_LT(kInvalidId, window_num);
    const SessionTab* tab;
    if (!associator->GetForeignTab(session_string_value, tab_id, &tab)) {
      LOG(ERROR) << "Failed to load foreign tab.";
      return;
    }
    WindowOpenDisposition disposition =
        web_ui_util::GetDispositionFromClick(args, 3);
    SessionRestore::RestoreForeignSessionTab(profile, *tab, disposition);
  } else {
    std::vector<const SessionWindow*> windows;
    // Note: we don't own the ForeignSessions themselves.
    if (!associator->GetForeignSession(session_string_value, &windows)) {
      LOG(ERROR) << "ForeignSessionHandler failed to get session data from"
          "SessionModelAssociator.";
      return;
    }
    std::vector<const SessionWindow*>::const_iterator iter_begin =
        windows.begin() + ((window_num == kInvalidId) ? 0 : window_num);
    std::vector<const SessionWindow*>::const_iterator iter_end =
        ((window_num == kInvalidId) ?
        std::vector<const SessionWindow*>::const_iterator(windows.end()) :
        iter_begin + 1);
    SessionRestore::RestoreForeignSessionWindows(profile, iter_begin, iter_end);
  }
}

void ForeignSessionHandler::HandleDeleteForeignSession(const ListValue* args) {
  if (args->GetSize() != 1U) {
    LOG(ERROR) << "Wrong number of args to deleteForeignSession";
    return;
  }

  // Get the session tag argument (required).
  std::string session_tag = UTF16ToUTF8(ExtractStringValue(args));

  SessionModelAssociator* associator = GetModelAssociator();
  if (associator)
    associator->DeleteForeignSession(session_tag);
}

bool ForeignSessionHandler::SessionTabToValue(
    const SessionTab& tab,
    DictionaryValue* dictionary) {
  if (tab.navigations.empty())
    return false;
  int selected_index = tab.current_navigation_index;
  selected_index = std::max(
      0,
      std::min(selected_index,
               static_cast<int>(tab.navigations.size() - 1)));
  const TabNavigation& current_navigation =
      tab.navigations.at(selected_index);
  GURL tab_url = current_navigation.virtual_url();
  if (tab_url == GURL(chrome::kChromeUINewTabURL))
    return false;
  NewTabUI::SetURLTitleAndDirection(dictionary, current_navigation.title(),
                                    tab_url);
  dictionary->SetString("type", "tab");
  dictionary->SetDouble("timestamp",
                        static_cast<double>(tab.timestamp.ToInternalValue()));
  dictionary->SetInteger("sessionId", tab.tab_id.id());
  return true;
}

bool ForeignSessionHandler::SessionWindowToValue(
    const SessionWindow& window,
    DictionaryValue* dictionary) {
  if (window.tabs.empty()) {
    NOTREACHED();
    return false;
  }
  scoped_ptr<ListValue> tab_values(new ListValue());
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    scoped_ptr<DictionaryValue> tab_value(new DictionaryValue());
    if (SessionTabToValue(*window.tabs[i], tab_value.get()))
      tab_values->Append(tab_value.release());
  }
  if (tab_values->GetSize() == 0)
    return false;
  dictionary->SetString("type", "window");
  dictionary->SetDouble("timestamp",
      static_cast<double>(window.timestamp.ToInternalValue()));
  dictionary->SetInteger("sessionId", window.window_id.id());
  dictionary->Set("tabs", tab_values.release());
  return true;
}

}  // namespace browser_sync
