// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/foreign_session_handler.h"

#include <algorithm>
#include <string>
#include <vector>
#include "base/scoped_vector.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/webui/new_tab_ui.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"

namespace browser_sync {

// Maximum number of session we're going to display on the NTP
static const int kMaxSessionsToShow = 10;

// Invalid value, used to note that we don't have a tab or window number.
static const int kInvalidId = -1;

ForeignSessionHandler::ForeignSessionHandler() {
  Init();
}

void ForeignSessionHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getForeignSessions",
      NewCallback(this,
      &ForeignSessionHandler::HandleGetForeignSessions));
  web_ui_->RegisterMessageCallback("openForeignSession",
      NewCallback(this,
      &ForeignSessionHandler::HandleOpenForeignSession));
}

void ForeignSessionHandler::Init() {
  registrar_.Add(this, NotificationType::SYNC_CONFIGURE_DONE,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::FOREIGN_SESSION_UPDATED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::FOREIGN_SESSION_DISABLED,
                 NotificationService::AllSources());
}

void ForeignSessionHandler::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  ListValue list_value;
  switch (type.value) {
    case NotificationType::SYNC_CONFIGURE_DONE:
    case NotificationType::FOREIGN_SESSION_UPDATED:
      HandleGetForeignSessions(&list_value);
      break;
    case NotificationType::FOREIGN_SESSION_DISABLED:
      // Calling foreignSessions with empty list will automatically hide
      // foreign session section.
      web_ui_->CallJavascriptFunction(L"foreignSessions", list_value);
      break;
    default:
      NOTREACHED();
  }
}

SessionModelAssociator* ForeignSessionHandler::GetModelAssociator() {
  ProfileSyncService* service = web_ui_->GetProfile()->GetProfileSyncService();
  if (service == NULL)
    return NULL;
  // We only want to set the model associator if there is one, and it is done
  // syncing sessions.
  SessionModelAssociator* model_associator = service->
      GetSessionModelAssociator();
  if (model_associator == NULL ||
      !service->ShouldPushChanges()) {
    return NULL;
  }
  return web_ui_->GetProfile()->GetProfileSyncService()->
      GetSessionModelAssociator();
}

void ForeignSessionHandler::HandleGetForeignSessions(const ListValue* args) {
  SessionModelAssociator* associator = GetModelAssociator();
  std::vector<const ForeignSession*> sessions;

  if (associator == NULL) {
    // Called before associator created, exit.
    return;
  }

  // Note: we don't own the ForeignSessions themselves.
  if (!associator->GetAllForeignSessions(&sessions)) {
    LOG(ERROR) << "ForeignSessionHandler failed to get session data from"
        "SessionModelAssociator.";
    return;
  }
  int added_count = 0;
  ListValue session_list;
  for (std::vector<const ForeignSession*>::const_iterator i =
      sessions.begin(); i != sessions.end() &&
      added_count < kMaxSessionsToShow; ++i) {
    const ForeignSession* foreign_session = *i;
    scoped_ptr<ListValue> window_list(new ListValue());
    for (std::vector<SessionWindow*>::const_iterator it =
        foreign_session->windows.begin(); it != foreign_session->windows.end();
        ++it) {
      SessionWindow* window = *it;
      scoped_ptr<DictionaryValue> window_data(new DictionaryValue());
      if (SessionWindowToValue(*window, window_data.get())) {
        window_data->SetString("sessionTag",
            foreign_session->foreign_session_tag);

        // Give ownership to |list_value|.
        window_list->Append(window_data.release());
      }
    }
    added_count++;

    // Give ownership to |session_list|
    session_list.Append(window_list.release());
  }
  web_ui_->CallJavascriptFunction(L"foreignSessions", session_list);
}

void ForeignSessionHandler::HandleOpenForeignSession(
    const ListValue* args) {
  size_t num_args = args->GetSize();
  if (num_args > 3U || num_args == 0) {
    LOG(ERROR) << "openForeignWindow called with only " << args->GetSize()
               << " arguments.";
    return;
  }

  // Extract the machine tag (always provided)
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
  if (num_args == 3 && (!args->GetString(2, &tab_id_str) ||
      !base::StringToInt(tab_id_str, &tab_id))) {
    LOG(ERROR) << "Failed to extract tab SessionID.";
    return;
  }

  SessionModelAssociator* associator = GetModelAssociator();

  if (tab_id != kInvalidId) {
    // We don't actually care about |window_num|, this is just a sanity check.
    DCHECK_LT(kInvalidId, window_num);
    const SessionTab* tab;
    if (!associator->GetForeignTab(session_string_value, tab_id, &tab)) {
      LOG(ERROR) << "Failed to load foreign tab.";
      return;
    }
    SessionRestore::RestoreForeignSessionTab(web_ui_->GetProfile(), *tab);
  } else {
    std::vector<SessionWindow*> windows;
    // Note: we don't own the ForeignSessions themselves.
    if (!associator->GetForeignSession(session_string_value, &windows)) {
      LOG(ERROR) << "ForeignSessionHandler failed to get session data from"
          "SessionModelAssociator.";
      return;
    }
    std::vector<SessionWindow*>::const_iterator iter_begin = windows.begin() +
        ((window_num == kInvalidId) ? 0 : window_num);
    std::vector<SessionWindow*>::const_iterator iter_end =
        ((window_num == kInvalidId) ?
        std::vector<SessionWindow*>::const_iterator(windows.end()) :
        iter_begin+1);
    SessionRestore::RestoreForeignSessionWindows(web_ui_->GetProfile(),
                                                 iter_begin,
                                                 iter_end);
  }
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
  if (current_navigation.virtual_url() == GURL(chrome::kChromeUINewTabURL))
    return false;
  NewTabUI::SetURLTitleAndDirection(dictionary, current_navigation.title(),
                                    current_navigation.virtual_url());
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
