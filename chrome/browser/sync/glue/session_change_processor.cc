// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/session_change_processor.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/scoped_vector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"

namespace browser_sync {

SessionChangeProcessor::SessionChangeProcessor(
    UnrecoverableErrorHandler* error_handler,
    SessionModelAssociator* session_model_associator)
    : ChangeProcessor(error_handler),
      session_model_associator_(session_model_associator),
      profile_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error_handler);
  DCHECK(session_model_associator_);
}

SessionChangeProcessor::SessionChangeProcessor(
    UnrecoverableErrorHandler* error_handler,
    SessionModelAssociator* session_model_associator,
    bool setup_for_test)
    : ChangeProcessor(error_handler),
      session_model_associator_(session_model_associator),
      profile_(NULL),
      setup_for_test_(setup_for_test) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error_handler);
  DCHECK(session_model_associator_);
}

SessionChangeProcessor::~SessionChangeProcessor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void SessionChangeProcessor::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(running());
  DCHECK(profile_);

  // Track which windows and/or tabs are modified.
  std::vector<TabContents*> modified_tabs;
  bool windows_changed = false;
  switch (type.value) {
    case NotificationType::BROWSER_OPENED: {
      Browser* browser = Source<Browser>(source).ptr();
      if (browser->profile() != profile_) {
        return;
      }

      windows_changed = true;
      break;
    }

    case NotificationType::TAB_PARENTED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      if (controller->profile() != profile_) {
        return;
      }
      windows_changed = true;
      modified_tabs.push_back(controller->tab_contents());
      break;
    }

    case NotificationType::TAB_CLOSED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      if (controller->profile() != profile_) {
        return;
      }
      windows_changed = true;
      modified_tabs.push_back(controller->tab_contents());
      break;
    }

    case NotificationType::NAV_LIST_PRUNED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      if (controller->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(controller->tab_contents());
      break;
    }

    case NotificationType::NAV_ENTRY_CHANGED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      if (controller->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(controller->tab_contents());
      break;
    }

    case NotificationType::NAV_ENTRY_COMMITTED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      if (controller->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(controller->tab_contents());
      break;
    }

    case NotificationType::TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED: {
      TabContents* tab_contents = Source<TabContents>(source).ptr();
      DCHECK(tab_contents);
      if (tab_contents->profile() != profile_) {
        return;
      }
      if (tab_contents->extension_app()) {
        modified_tabs.push_back(tab_contents);
      }
      break;
    }
    default:
      LOG(ERROR) << "Received unexpected notification of type "
                  << type.value;
      break;
  }

  // Associate windows first to ensure tabs have homes.
  if (windows_changed)
    session_model_associator_->ReassociateWindows(false);
  if (!modified_tabs.empty())
    session_model_associator_->ReassociateTabs(modified_tabs);
}

void SessionChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!running()) {
    return;
  }

  StopObserving();

  sync_api::ReadNode root(trans);
  if (!root.InitByTagLookup(kSessionsTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Sessions root node lookup failed.");
    return;
  }

  for (int i = 0; i < change_count; ++i) {
    const sync_api::SyncManager::ChangeRecord& change = changes[i];
    sync_api::SyncManager::ChangeRecord::Action action(change.action);
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE == action) {
      // Deletions should only be for a foreign client itself, and hence affect
      // the header node, never a tab node.
      sync_api::ReadNode node(trans);
      if (!node.InitByIdLookup(change.id)) {
        error_handler()->OnUnrecoverableError(FROM_HERE,
                                              "Session node lookup failed.");
        return;
      }
      DCHECK_EQ(node.GetModelType(), syncable::SESSIONS);
      const sync_pb::SessionSpecifics& specifics = node.GetSessionSpecifics();
      session_model_associator_->DisassociateForeignSession(
          specifics.session_tag());
      continue;
    }

    // Handle an update or add.
    sync_api::ReadNode sync_node(trans);
    if (!sync_node.InitByIdLookup(change.id)) {
      error_handler()->OnUnrecoverableError(FROM_HERE,
          "Session node lookup failed.");
      return;
    }

    // Check that the changed node is a child of the session folder.
    DCHECK(root.GetId() == sync_node.GetParentId());
    DCHECK(syncable::SESSIONS == sync_node.GetModelType());

    const sync_pb::SessionSpecifics& specifics(
        sync_node.GetSessionSpecifics());
    if (specifics.session_tag() ==
            session_model_associator_->GetCurrentMachineTag() &&
        !setup_for_test_) {
      // We should only ever receive a change to our own machine's session info
      // if encryption was turned on. In that case, the data is still the same,
      // so we can ignore.
      LOG(WARNING) << "Dropping modification to local session.";
      return;
    }
    const int64 mtime = sync_node.GetModificationTime();
    // Model associator handles foreign session update and add the same.
    session_model_associator_->AssociateForeignSpecifics(specifics, mtime);
  }

  // Notify foreign session handlers that there are new sessions.
  NotificationService::current()->Notify(
      NotificationType::FOREIGN_SESSION_UPDATED,
      NotificationService::AllSources(),
      NotificationService::NoDetails());

  StartObserving();
}

void SessionChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  DCHECK(profile_ == NULL);
  profile_ = profile;
  StartObserving();
}

void SessionChangeProcessor::StopImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  StopObserving();
  profile_ = NULL;
}

void SessionChangeProcessor::StartObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  notification_registrar_.Add(this, NotificationType::TAB_PARENTED,
      NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::TAB_CLOSED,
      NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::NAV_LIST_PRUNED,
      NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::NAV_ENTRY_CHANGED,
      NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
      NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::BROWSER_OPENED,
      NotificationService::AllSources());
  notification_registrar_.Add(this,
      NotificationType::TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED,
      NotificationService::AllSources());
}

void SessionChangeProcessor::StopObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  notification_registrar_.RemoveAll();
}

}  // namespace browser_sync
