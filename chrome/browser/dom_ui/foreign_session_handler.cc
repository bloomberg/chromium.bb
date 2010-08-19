// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/foreign_session_handler.h"

#include "base/scoped_vector.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/dom_ui/value_helper.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"

namespace browser_sync {

static const int kMaxSessionsToShow = 10;

ForeignSessionHandler::ForeignSessionHandler() {
  Init();
}

void ForeignSessionHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getForeignSessions",
      NewCallback(this,
      &ForeignSessionHandler::HandleGetForeignSessions));
  dom_ui_->RegisterMessageCallback("reopenForeignSession",
      NewCallback(this,
      &ForeignSessionHandler::HandleReopenForeignSession));
}

void ForeignSessionHandler::Init() {
  registrar_.Add(this, NotificationType::SYNC_CONFIGURE_DONE,
      NotificationService::AllSources());
  registrar_.Add(this, NotificationType::FOREIGN_SESSION_UPDATED,
      NotificationService::AllSources());
  registrar_.Add(this, NotificationType::FOREIGN_SESSION_DELETED,
      NotificationService::AllSources());
}

void ForeignSessionHandler::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  if (type != NotificationType::SYNC_CONFIGURE_DONE &&
      type != NotificationType::FOREIGN_SESSION_UPDATED &&
      type != NotificationType::FOREIGN_SESSION_DELETED) {
    NOTREACHED();
    return;
  }
  ListValue list_value;
  HandleGetForeignSessions(&list_value);
}

SessionModelAssociator* ForeignSessionHandler::GetModelAssociator() {
  ProfileSyncService* service = dom_ui_->GetProfile()->GetProfileSyncService();
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
  return dom_ui_->GetProfile()->GetProfileSyncService()->
      GetSessionModelAssociator();
}

void ForeignSessionHandler::HandleGetForeignSessions(const ListValue* args) {
  SessionModelAssociator* associator = GetModelAssociator();
  if (associator)
    GetForeignSessions(associator);
}

void ForeignSessionHandler::HandleReopenForeignSession(
    const ListValue* args) {
  // Extract the machine tag and use it to obtain the id for the node we are
  // looking for.  Send it along with a valid associator to OpenForeignSessions.
  std::string session_string_value = WideToUTF8(ExtractStringValue(args));
  SessionModelAssociator* associator = GetModelAssociator();
  if (associator && !session_string_value.empty()) {
    int64 id = associator->GetSyncIdFromChromeId(session_string_value);
    OpenForeignSession(associator, id);
  }
}

void ForeignSessionHandler::OpenForeignSession(
    SessionModelAssociator* associator, int64 id) {
  // Obtain the session windows for the foreign session.
  // We don't have a ForeignSessionHandler in off the record mode, so we
  // expect the ProfileSyncService to exist.
  sync_api::ReadTransaction trans(dom_ui_->GetProfile()->
      GetProfileSyncService()->backend()->GetUserShareHandle());
  ScopedVector<ForeignSession> session;
  associator->AppendForeignSessionWithID(id, &session.get(), &trans);

  DCHECK(session.size() == 1);
  std::vector<SessionWindow*> windows = (*session.begin())->windows;
  SessionRestore::RestoreForeignSessionWindows(dom_ui_->GetProfile(), &windows);
}

void ForeignSessionHandler::GetForeignSessions(
    SessionModelAssociator* associator) {
  ScopedVector<ForeignSession> windows;
  associator->GetSessionDataFromSyncModel(&windows.get());
  int added_count = 0;
  ListValue list_value;
  for (std::vector<ForeignSession*>::const_iterator i =
      windows.begin(); i != windows.end() &&
      added_count < kMaxSessionsToShow; ++i) {
    ForeignSession* foreign_session = *i;
    std::vector<TabRestoreService::Entry*> entries;
    dom_ui_->GetProfile()->GetTabRestoreService()->CreateEntriesFromWindows(
        &foreign_session->windows, &entries);
    for (std::vector<TabRestoreService::Entry*>::const_iterator it =
        entries.begin(); it != entries.end(); ++it) {
      TabRestoreService::Entry* entry = *it;
      scoped_ptr<DictionaryValue> value(new DictionaryValue());
      if (entry->type == TabRestoreService::WINDOW &&
          ValueHelper::WindowToValue(
              *static_cast<TabRestoreService::Window*>(entry), value.get())) {
        // The javascript checks if the session id is a valid session id,
        // when rendering session information to the new tab page, and it
        // sends the sessionTag back when we need to restore a session.
        value->SetString("sessionTag", foreign_session->foreign_tession_tag);
        value->SetInteger("sessionId", entry->id);
        list_value.Append(value.release());  // Give ownership to |list_value|.
      }
    }
    added_count++;
  }
  dom_ui_->CallJavascriptFunction(L"foreignSessions", list_value);
}

}  // namespace browser_sync

