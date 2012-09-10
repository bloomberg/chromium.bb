// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/session_change_processor.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_state_map.h"
#include "sync/internal_api/public/change_record.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/protocol/session_specifics.pb.h"

using content::BrowserThread;
using content::NavigationController;
using content::WebContents;

namespace browser_sync {

namespace {

// The URL at which the set of synced tabs is displayed. We treat it differently
// from all other URL's as accessing it triggers a sync refresh of Sessions.
static const char kNTPOpenTabSyncURL[] = "chrome://newtab/#opentabs";

// Extract the source SyncedTabDelegate from a NotificationSource originating
// from a NavigationController, if it exists. Returns |NULL| otherwise.
SyncedTabDelegate* ExtractSyncedTabDelegate(
    const content::NotificationSource& source) {
  TabContents* tab = TabContents::FromWebContents(
      content::Source<NavigationController>(source).ptr()->GetWebContents());
  if (!tab)
    return NULL;
  return tab->synced_tab_delegate();
}

}  // namespace

SessionChangeProcessor::SessionChangeProcessor(
    DataTypeErrorHandler* error_handler,
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
    DataTypeErrorHandler* error_handler,
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
    case chrome::NOTIFICATION_FAVICON_CHANGED: {
      content::Details<history::FaviconChangeDetails> favicon_details(details);
      session_model_associator_->FaviconsUpdated(favicon_details->urls);
      // Note: we favicon notifications don't affect tab contents, so we return
      // here instead of continuing on to reassociate tabs/windows.
      return;
    }

    case chrome::NOTIFICATION_BROWSER_OPENED: {
      Browser* browser = content::Source<Browser>(source).ptr();
      if (!browser || browser->profile() != profile_) {
        return;
      }
      DVLOG(1) << "Received BROWSER_OPENED for profile " << profile_;
      break;
    }

    case chrome::NOTIFICATION_TAB_PARENTED: {
      SyncedTabDelegate* tab =
          content::Source<TabContents>(source).ptr()->synced_tab_delegate();
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      DVLOG(1) << "Received TAB_PARENTED for profile " << profile_;
      break;
    }

    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME: {
      TabContents* tab_contents = TabContents::FromWebContents(
          content::Source<WebContents>(source).ptr());
      if (!tab_contents) {
        return;
      }
      SyncedTabDelegate* tab = tab_contents->synced_tab_delegate();
      if (!tab || tab->profile() != profile_) {
        return;
      }
      modified_tabs.push_back(tab);
      DVLOG(1) << "Received LOAD_COMPLETED_MAIN_FRAME for profile " << profile_;
      break;
    }

    case chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED: {
      TabContents* tab_contents = content::Source<TabContents>(source).ptr();
      SyncedTabDelegate* tab = tab_contents->synced_tab_delegate();
      if (!tab || tab->profile() != profile_)
        return;
      modified_tabs.push_back(tab);
      DVLOG(1) << "Received NOTIFICATION_TAB_CONTENTS_DESTROYED for profile "
               << profile_;
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
      extensions::TabHelper* extension_tab_helper =
          content::Source<extensions::TabHelper>(source).ptr();
      if (extension_tab_helper->web_contents()->GetBrowserContext() !=
              profile_) {
        return;
      }
      if (extension_tab_helper->extension_app()) {
        TabContents* tab_contents =
            TabContents::FromWebContents(extension_tab_helper->web_contents());
        modified_tabs.push_back(tab_contents->synced_tab_delegate());
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

  // Check if this tab should trigger a session sync refresh. By virtue of
  // it being a modified tab, we know the tab is active (so we won't do
  // refreshes just because the refresh page is open in a background tab).
  if (!modified_tabs.empty()) {
    SyncedTabDelegate* tab = modified_tabs.front();
    const content::NavigationEntry* entry = tab->GetActiveEntry();
    if (!tab->IsBeingDestroyed() &&
        entry &&
        entry->GetVirtualURL().is_valid() &&
        entry->GetVirtualURL().spec() == kNTPOpenTabSyncURL) {
      DVLOG(1) << "Triggering sync refresh for sessions datatype.";
      const syncer::ModelType type = syncer::SESSIONS;
      syncer::ModelTypeStateMap state_map;
      state_map.insert(std::make_pair(type, syncer::InvalidationState()));
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
          content::Source<Profile>(profile_),
          content::Details<const syncer::ModelTypeStateMap>(&state_map));
    }
  }

  // Associate tabs first so the synced session tracker is aware of them.
  // Note that if we fail to associate, it means something has gone wrong,
  // such as our local session being deleted, so we disassociate and associate
  // again.
  bool reassociation_needed = !modified_tabs.empty() &&
      !session_model_associator_->AssociateTabs(modified_tabs, NULL);

  // Note, we always associate windows because it's possible a tab became
  // "interesting" by going to a valid URL, in which case it needs to be added
  // to the window's tab information.
  if (!reassociation_needed) {
    reassociation_needed =
        !session_model_associator_->AssociateWindows(false, NULL);
  }

  if (reassociation_needed) {
    LOG(WARNING) << "Reassociation of local models triggered.";
    syncer::SyncError error;
    error = session_model_associator_->DisassociateModels();
    error = session_model_associator_->AssociateModels();
    if (error.IsSet()) {
      error_handler()->OnSingleDatatypeUnrecoverableError(
          error.location(),
          error.message());
    }
  }
}

void SessionChangeProcessor::ApplyChangesFromSyncModel(
    const syncer::BaseTransaction* trans,
    const syncer::ImmutableChangeRecordList& changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!running()) {
    return;
  }

  ScopedStopObserving<SessionChangeProcessor> stop_observing(this);

  syncer::ReadNode root(trans);
  if (root.InitByTagLookup(kSessionsTag) != syncer::BaseNode::INIT_OK) {
    error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
        "Sessions root node lookup failed.");
    return;
  }

  std::string local_tag = session_model_associator_->GetCurrentMachineTag();
  for (syncer::ChangeRecordList::const_iterator it =
           changes.Get().begin(); it != changes.Get().end(); ++it) {
    const syncer::ChangeRecord& change = *it;
    syncer::ChangeRecord::Action action(change.action);
    if (syncer::ChangeRecord::ACTION_DELETE == action) {
      // Deletions are all or nothing (since we only ever delete entire
      // sessions). Therefore we don't care if it's a tab node or meta node,
      // and just ensure we've disassociated.
      DCHECK_EQ(syncer::GetModelTypeFromSpecifics(it->specifics),
                syncer::SESSIONS);
      const sync_pb::SessionSpecifics& specifics = it->specifics.session();
      if (specifics.session_tag() == local_tag) {
        // Another client has attempted to delete our local data (possibly by
        // error or their/our clock is inaccurate). Just ignore the deletion
        // for now to avoid any possible ping-pong delete/reassociate sequence.
        LOG(WARNING) << "Local session data deleted. Ignoring until next local "
                     << "navigation event.";
      } else {
        session_model_associator_->DisassociateForeignSession(
            specifics.session_tag());
      }
      continue;
    }

    // Handle an update or add.
    syncer::ReadNode sync_node(trans);
    if (sync_node.InitByIdLookup(change.id) != syncer::BaseNode::INIT_OK) {
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
          "Session node lookup failed.");
      return;
    }

    // Check that the changed node is a child of the session folder.
    DCHECK(root.GetId() == sync_node.GetParentId());
    DCHECK(syncer::SESSIONS == sync_node.GetModelType());

    const sync_pb::SessionSpecifics& specifics(
        sync_node.GetSessionSpecifics());
    if (specifics.session_tag() == local_tag &&
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
  if (!profile_)
    return;
  notification_registrar_.Add(this, chrome::NOTIFICATION_TAB_PARENTED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED,
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
  notification_registrar_.Add(this, chrome::NOTIFICATION_FAVICON_CHANGED,
      content::Source<Profile>(profile_));
}

void SessionChangeProcessor::StopObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  notification_registrar_.RemoveAll();
}

}  // namespace browser_sync
