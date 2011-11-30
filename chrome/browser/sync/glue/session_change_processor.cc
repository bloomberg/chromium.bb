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
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/internal_api/change_record.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/sync/tab_contents_wrapper_synced_tab_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace browser_sync {

namespace {

// Extract the source SyncedTabDelegate from a NotificationSource originating
// from a NavigationController, if it exists. Returns |NULL| otherwise.
SyncedTabDelegate* ExtractSyncedTabDelegate(
    const content::NotificationSource& source) {
  TabContentsWrapper* tab = TabContentsWrapper::GetCurrentWrapperForContents(
      content::Source<NavigationController>(source).ptr()->tab_contents());
  if (!tab)
    return NULL;
  return tab->synced_tab_delegate();
}

}  // namespace

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

void SessionChangeProcessor::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(running());
  DCHECK(profile_);

  // Track which windows and/or tabs are modified.
  std::vector<SyncedTabDelegate*> modified_tabs;
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_OPENED: {
      Browser* browser = content::Source<Browser>(source).ptr();
      if (!browser || browser->profile() != profile_) {
        return;
      }
      DVLOG(1) << "Received BROWSER_OPENED for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_TAB_PARENTED: {
      SyncedTabDelegate* tab = content::Source<SyncedTabDelegate>(source).ptr();
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      DVLOG(1) << "Received TAB_PARENTED for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME: {
      TabContentsWrapper* tab_contents_wrapper =
          TabContentsWrapper::GetCurrentWrapperForContents(
              content::Source<TabContents>(source).ptr());
      if (!tab_contents_wrapper) {
        return;
      }
      SyncedTabDelegate* tab = tab_contents_wrapper->synced_tab_delegate();
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      DVLOG(1) << "Received LOAD_COMPLETED_MAIN_FRAME for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_TAB_CLOSED: {
      SyncedTabDelegate* tab = ExtractSyncedTabDelegate(source);
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      DVLOG(1) << "Received TAB_CLOSED for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_NAV_LIST_PRUNED: {
      SyncedTabDelegate* tab = ExtractSyncedTabDelegate(source);
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      DVLOG(1) << "Received NAV_LIST_PRUNED for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_NAV_ENTRY_CHANGED: {
      SyncedTabDelegate* tab = ExtractSyncedTabDelegate(source);
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      DVLOG(1) << "Received NAV_ENTRY_CHANGED for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      SyncedTabDelegate* tab = ExtractSyncedTabDelegate(source);
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      DVLOG(1) << "Received NAV_ENTRY_COMMITTED for profile " << profile_;
      break;
    }

    case chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED: {
      ExtensionTabHelper* extension_tab_helper =
          content::Source<ExtensionTabHelper>(source).ptr();
      if (!extension_tab_helper ||
          extension_tab_helper->tab_contents()->browser_context() != profile_) {
        return;
      }
      if (extension_tab_helper->extension_app()) {
        modified_tabs.push_back(extension_tab_helper->tab_contents_wrapper()->
            synced_tab_delegate());
      }
      DVLOG(1) << "Received TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED "
               << "for profile " << profile_;
      break;
    }

    default:
      LOG(ERROR) << "Received unexpected notification of type "
                  << type;
      break;
  }

  // Associate tabs first so the synced session tracker is aware of them.
  // Note that if we fail to associate, it means something has gone wrong,
  // such as our local session being deleted, so we disassociate and associate
  // again.
  bool reassociation_needed = !modified_tabs.empty() &&
      !session_model_associator_->AssociateTabs(modified_tabs);

  // Note, we always associate windows because it's possible a tab became
  // "interesting" by going to a valid URL, in which case it needs to be added
  // to the window's tab information.
  if (!reassociation_needed) {
    reassociation_needed =
        !session_model_associator_->AssociateWindows(false);
  }

  if (reassociation_needed) {
    DVLOG(1) << "Reassociation of local models triggered.";
    SyncError error;
    session_model_associator_->DisassociateModels(&error);
    session_model_associator_->AssociateModels(&error);
    if (error.IsSet()) {
      error_handler()->OnUnrecoverableError(FROM_HERE,
          "Sessions reassociation failed.");
    }
  }
}

void SessionChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::ImmutableChangeRecordList& changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!running()) {
    return;
  }

  ScopedStopObserving<SessionChangeProcessor> stop_observing(this);

  sync_api::ReadNode root(trans);
  if (!root.InitByTagLookup(kSessionsTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Sessions root node lookup failed.");
    return;
  }

  for (sync_api::ChangeRecordList::const_iterator it =
           changes.Get().begin(); it != changes.Get().end(); ++it) {
    const sync_api::ChangeRecord& change = *it;
    sync_api::ChangeRecord::Action action(change.action);
    if (sync_api::ChangeRecord::ACTION_DELETE == action) {
      // Deletions are all or nothing (since we only ever delete entire
      // sessions). Therefore we don't care if it's a tab node or meta node,
      // and just ensure we've disassociated.
      DCHECK_EQ(syncable::GetModelTypeFromSpecifics(it->specifics),
                syncable::SESSIONS);
      const sync_pb::SessionSpecifics& specifics =
          it->specifics.GetExtension(sync_pb::session);
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
    const base::Time& mtime = sync_node.GetModificationTime();
    // The model associator handles foreign session updates and adds the same.
    session_model_associator_->AssociateForeignSpecifics(specifics, mtime);
  }

  // Notify foreign session handlers that there are new sessions.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
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
      content::NotificationService::AllSources());
  notification_registrar_.Add(this, content::NOTIFICATION_TAB_CLOSED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(this, content::NOTIFICATION_NAV_LIST_PRUNED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_CHANGED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
      content::NotificationService::AllBrowserContextsAndSources());
  notification_registrar_.Add(this,
      chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(this,
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllBrowserContextsAndSources());
}

void SessionChangeProcessor::StopObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  notification_registrar_.RemoveAll();
}

}  // namespace browser_sync
