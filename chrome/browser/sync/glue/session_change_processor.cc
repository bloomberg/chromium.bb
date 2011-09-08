// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/session_change_processor.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/sync_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"

namespace browser_sync {

namespace {
// Extract the source SyncedTabDelegate from a NotificationSource originating
// from a NavigationController, if it exists. Returns |NULL| otherwise.
SyncedTabDelegate* ExtractSyncedTabDelegate(const NotificationSource& source) {
  TabContentsWrapper* tab =  TabContentsWrapper::GetCurrentWrapperForContents(
      Source<NavigationController>(source).ptr()->tab_contents());
  if (!tab)
    return NULL;
  return tab->synced_tab_delegate();
}
}

SessionChangeProcessor::SessionChangeProcessor(
    UnrecoverableErrorHandler* error_handler,
    SessionModelAssociator* session_model_associator)
    : ChangeProcessor(error_handler),
      session_model_associator_(session_model_associator),
      profile_(NULL),
      setup_for_test_(false) {
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

void SessionChangeProcessor::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(running());
  DCHECK(profile_);

  // Track which windows and/or tabs are modified.
  std::vector<SyncedTabDelegate*> modified_tabs;
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_OPENED: {
      Browser* browser = Source<Browser>(source).ptr();
      if (!browser || browser->profile() != profile_) {
        return;
      }
      VLOG(1) << "Received BROWSER_OPENED for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_TAB_PARENTED: {
      SyncedTabDelegate* tab = Source<SyncedTabDelegate>(source).ptr();
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      VLOG(1) << "Received TAB_PARENTED for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME: {
      TabContentsWrapper* tab_contents_wrapper =
          TabContentsWrapper::GetCurrentWrapperForContents(
              Source<TabContents>(source).ptr());
      if (!tab_contents_wrapper) {
        return;
      }
      SyncedTabDelegate* tab = tab_contents_wrapper->synced_tab_delegate();
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      VLOG(1) << "Received LOAD_COMPLETED_MAIN_FRAME for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_TAB_CLOSED: {
      SyncedTabDelegate* tab = ExtractSyncedTabDelegate(source);
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      VLOG(1) << "Received TAB_CLOSED for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_NAV_LIST_PRUNED: {
      SyncedTabDelegate* tab = ExtractSyncedTabDelegate(source);
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      VLOG(1) << "Received NAV_LIST_PRUNED for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_NAV_ENTRY_CHANGED: {
      SyncedTabDelegate* tab = ExtractSyncedTabDelegate(source);
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      VLOG(1) << "Received NAV_ENTRY_CHANGED for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      SyncedTabDelegate* tab = ExtractSyncedTabDelegate(source);
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      VLOG(1) << "Received NAV_ENTRY_COMMITTED for profile " << profile_;
      break;
    }

    case chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED: {
      ExtensionTabHelper* extension_tab_helper =
          Source<ExtensionTabHelper>(source).ptr();
      if (!extension_tab_helper ||
          extension_tab_helper->tab_contents()->browser_context() != profile_) {
        return;
      }
      if (extension_tab_helper->extension_app()) {
        modified_tabs.push_back(extension_tab_helper->tab_contents_wrapper()->
            synced_tab_delegate());
      }
      VLOG(1) << "Received TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED "
              << "for profile " << profile_;
      break;
    }

    default:
      LOG(ERROR) << "Received unexpected notification of type "
                  << type;
      break;
  }

  // Associate tabs first so the synced session tracker is aware of them.
  if (!modified_tabs.empty())
    session_model_associator_->ReassociateTabs(modified_tabs);
  // Note, we always reassociate windows because it's possible a tab became
  // "interesting" by going to a valid URL, in which case it needs to be added
  // to the window's tab information.
  session_model_associator_->ReassociateWindows(false);
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
      StartObserving();
      return;
    }
    const int64 mtime = sync_node.GetModificationTime();
    // Model associator handles foreign session update and add the same.
    session_model_associator_->AssociateForeignSpecifics(specifics, mtime);
  }

  // Notify foreign session handlers that there are new sessions.
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED,
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
  notification_registrar_.Add(this, content::NOTIFICATION_TAB_PARENTED,
      NotificationService::AllSources());
  notification_registrar_.Add(this, content::NOTIFICATION_TAB_CLOSED,
      NotificationService::AllSources());
  notification_registrar_.Add(this, content::NOTIFICATION_NAV_LIST_PRUNED,
      NotificationService::AllSources());
  notification_registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_CHANGED,
      NotificationService::AllSources());
  notification_registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
      NotificationService::AllBrowserContextsAndSources());
  notification_registrar_.Add(this,
      chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED,
      NotificationService::AllSources());
  notification_registrar_.Add(this,
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      NotificationService::AllBrowserContextsAndSources());
}

void SessionChangeProcessor::StopObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  notification_registrar_.RemoveAll();
}

}  // namespace browser_sync
