// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions2/sessions_sync_manager.h"

#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_merge_result.h"
#include "sync/api/time.h"

using syncer::SyncChange;
using syncer::SyncData;

namespace browser_sync {

// Maximum number of favicons to sync.
// TODO(zea): pull this from the server.
static const int kMaxSyncFavicons = 200;

static const int kInvalidTabNodeId = -1;

SessionsSyncManager::SessionsSyncManager(
    Profile* profile,
    scoped_ptr<SyncPrefs> sync_prefs,
    SyncInternalApiDelegate* delegate)
    : favicon_cache_(profile, kMaxSyncFavicons),
      delegate_(delegate),
      local_session_header_node_id_(kInvalidTabNodeId) {
  sync_prefs_ = sync_prefs.Pass();
}

SessionsSyncManager::~SessionsSyncManager() {
}

// Returns the GUID-based string that should be used for
// |SessionsSyncManager::current_machine_tag_|.
static std::string BuildMachineTag(const std::string& cache_guid) {
  std::string machine_tag = "session_sync";
  machine_tag.append(cache_guid);
  return machine_tag;
}

syncer::SyncMergeResult SessionsSyncManager::MergeDataAndStartSyncing(
     syncer::ModelType type,
     const syncer::SyncDataList& initial_sync_data,
     scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
     scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  syncer::SyncMergeResult merge_result(type);
  DCHECK(session_tracker_.Empty());
  DCHECK_EQ(0U, local_tab_pool_.Capacity());

  error_handler_ = error_handler.Pass();
  sync_processor_ = sync_processor.Pass();

  local_session_header_node_id_ = kInvalidTabNodeId;
  scoped_ptr<DeviceInfo> local_device_info(delegate_->GetLocalDeviceInfo());
  syncer::SyncChangeList new_changes;

  // Make sure we have a machine tag.  We do this now (versus earlier) as it's
  // a conveniently safe time to assert sync is ready and the cache_guid is
  // initialized.
  if (current_machine_tag_.empty())
    InitializeCurrentMachineTag();
  if (local_device_info) {
    current_session_name_ = local_device_info->client_name();
  } else {
    merge_result.set_error(error_handler_->CreateAndUploadError(
        FROM_HERE,
        "Failed to get device info for machine tag."));
    return merge_result;
  }
  session_tracker_.SetLocalSessionTag(current_machine_tag_);

  // First, we iterate over sync data to update our session_tracker_.
  if (!InitFromSyncModel(initial_sync_data, &new_changes)) {
    // The sync db didn't have a header node for us. Create one.
    sync_pb::EntitySpecifics specifics;
    sync_pb::SessionSpecifics* base_specifics = specifics.mutable_session();
    base_specifics->set_session_tag(current_machine_tag());
    sync_pb::SessionHeader* header_s = base_specifics->mutable_header();
    header_s->set_client_name(current_session_name_);
    header_s->set_device_type(DeviceInfo::GetLocalDeviceType());
    syncer::SyncData data = syncer::SyncData::CreateLocalData(
        current_machine_tag(), current_session_name_, specifics);
    new_changes.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD, data));
  }

#if defined(OS_ANDROID)
  std::string sync_machine_tag(BuildMachineTag(delegate_->GetCacheGuid()));
  if (current_machine_tag_.compare(sync_machine_tag) != 0)
    DeleteForeignSession(sync_machine_tag, &new_changes);
#endif

  // Check if anything has changed on the local client side.
  AssociateWindows(RELOAD_TABS, &new_changes);

  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));
  return merge_result;
}

void SessionsSyncManager::AssociateWindows(
    ReloadTabsOption option,
    syncer::SyncChangeList* change_output) {
  NOTIMPLEMENTED() << "TODO(tim)";
}

void SessionsSyncManager::AssociateTab(SyncedTabDelegate* const tab,
                                       syncer::SyncChangeList* change_output) {
  NOTIMPLEMENTED() << "TODO(tim)";
}

void SessionsSyncManager::OnLocalTabModified(
    const SyncedTabDelegate& modified_tab, syncer::SyncError* error) {
  NOTIMPLEMENTED() << "TODO(tim): SessionModelAssociator::Observe equivalent.";
}

void SessionsSyncManager::OnBrowserOpened() {
  NOTIMPLEMENTED() << "TODO(tim): SessionModelAssociator::Observe equivalent.";
}

bool SessionsSyncManager::ShouldSyncTab(const SyncedTabDelegate& tab) const {
  NOTIMPLEMENTED() << "TODO(tim)";
  return true;
}

void SessionsSyncManager::ForwardRelevantFaviconUpdatesToFaviconCache(
    const std::set<GURL>& updated_favicon_page_urls) {
  NOTIMPLEMENTED() <<
      "TODO(tim): SessionModelAssociator::FaviconsUpdated equivalent.";
}

void SessionsSyncManager::StopSyncing(syncer::ModelType type) {
  NOTIMPLEMENTED();
}

syncer::SyncDataList SessionsSyncManager::GetAllSyncData(
    syncer::ModelType type) const {
  NOTIMPLEMENTED();
  return syncer::SyncDataList();
}

syncer::SyncError SessionsSyncManager::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  NOTIMPLEMENTED();
  return syncer::SyncError();
}

syncer::SyncChange SessionsSyncManager::TombstoneTab(
    const sync_pb::SessionSpecifics& tab) {
  if (!tab.has_tab_node_id()) {
    LOG(WARNING) << "Old sessions node without tab node id; can't tombstone.";
    return syncer::SyncChange();
  } else {
    return syncer::SyncChange(
        FROM_HERE,
        SyncChange::ACTION_DELETE,
        SyncData::CreateLocalDelete(
            TabNodePool2::TabIdToTag(current_machine_tag(),
                                     tab.tab_node_id()),
            syncer::SESSIONS));
  }
}

bool SessionsSyncManager::GetAllForeignSessions(
    std::vector<const SyncedSession*>* sessions) {
  return session_tracker_.LookupAllForeignSessions(sessions);
}

bool SessionsSyncManager::InitFromSyncModel(
    const syncer::SyncDataList& sync_data,
    syncer::SyncChangeList* new_changes) {
  bool found_current_header = false;
  for (syncer::SyncDataList::const_iterator it = sync_data.begin();
       it != sync_data.end();
       ++it) {
    const syncer::SyncData& data = *it;
    DCHECK(data.GetSpecifics().has_session());
    const sync_pb::SessionSpecifics& specifics = data.GetSpecifics().session();
    if (specifics.session_tag().empty() ||
           (specifics.has_tab() && (!specifics.has_tab_node_id() ||
                                    !specifics.tab().has_tab_id()))) {
      syncer::SyncChange tombstone(TombstoneTab(specifics));
      if (tombstone.IsValid())
        new_changes->push_back(tombstone);
    } else if (specifics.session_tag() != current_machine_tag()) {
      UpdateTrackerWithForeignSession(specifics, data.GetRemoteModifiedTime());
    } else {
      // This is previously stored local session information.
      if (specifics.has_header() && !found_current_header) {
        // This is our previous header node, reuse it.
        found_current_header = true;
        if (specifics.header().has_client_name())
          current_session_name_ = specifics.header().client_name();
      } else {
        if (specifics.has_header() || !specifics.has_tab()) {
          LOG(WARNING) << "Found more than one session header node with local "
                       << "tag.";
          syncer::SyncChange tombstone(TombstoneTab(specifics));
          if (tombstone.IsValid())
            new_changes->push_back(tombstone);
        } else {
          // This is a valid old tab node, add it to the pool so it can be
          // reused for reassociation.
          local_tab_pool_.AddTabNode(specifics.tab_node_id());
        }
      }
    }
  }
  return found_current_header;
}

void SessionsSyncManager::UpdateTrackerWithForeignSession(
    const sync_pb::SessionSpecifics& specifics,
    const base::Time& modification_time) {
  std::string foreign_session_tag = specifics.session_tag();
  DCHECK_NE(foreign_session_tag, current_machine_tag());

  SyncedSession* foreign_session =
      session_tracker_.GetSession(foreign_session_tag);
  if (specifics.has_header()) {
    // Read in the header data for this foreign session.
    // Header data contains window information and ordered tab id's for each
    // window.

    // Load (or create) the SyncedSession object for this client.
    const sync_pb::SessionHeader& header = specifics.header();
    PopulateSessionHeaderFromSpecifics(header,
                                       modification_time,
                                       foreign_session);

    // Reset the tab/window tracking for this session (must do this before
    // we start calling PutWindowInSession and PutTabInWindow so that all
    // unused tabs/windows get cleared by the CleanupSession(...) call).
    session_tracker_.ResetSessionTracking(foreign_session_tag);

    // Process all the windows and their tab information.
    int num_windows = header.window_size();
    DVLOG(1) << "Associating " << foreign_session_tag << " with "
             << num_windows << " windows.";

    for (int i = 0; i < num_windows; ++i) {
      const sync_pb::SessionWindow& window_s = header.window(i);
      SessionID::id_type window_id = window_s.window_id();
      session_tracker_.PutWindowInSession(foreign_session_tag,
                                          window_id);
      BuildSyncedSessionFromSpecifics(foreign_session_tag,
                                      window_s,
                                      modification_time,
                                      foreign_session->windows[window_id]);
    }
    // Delete any closed windows and unused tabs as necessary.
    session_tracker_.CleanupSession(foreign_session_tag);
  } else if (specifics.has_tab()) {
    const sync_pb::SessionTab& tab_s = specifics.tab();
    SessionID::id_type tab_id = tab_s.tab_id();
    SessionTab* tab =
        session_tracker_.GetTab(foreign_session_tag,
                                tab_id,
                                specifics.tab_node_id());

    // Update SessionTab based on protobuf.
    tab->SetFromSyncData(tab_s, modification_time);

    // If a favicon or favicon urls are present, load the URLs and visit
    // times into the in-memory favicon cache.
    RefreshFaviconVisitTimesFromForeignTab(tab_s, modification_time);

    // Update the last modified time.
    if (foreign_session->modified_time < modification_time)
      foreign_session->modified_time = modification_time;
  } else {
    LOG(WARNING) << "Ignoring foreign session node with missing header/tab "
                 << "fields and tag " << foreign_session_tag << ".";
  }
}

void SessionsSyncManager::InitializeCurrentMachineTag() {
  DCHECK(current_machine_tag_.empty());
  std::string persisted_guid;
  persisted_guid = sync_prefs_->GetSyncSessionsGUID();
  if (!persisted_guid.empty()) {
    current_machine_tag_ = persisted_guid;
    DVLOG(1) << "Restoring persisted session sync guid: " << persisted_guid;
  } else {
    current_machine_tag_ = BuildMachineTag(delegate_->GetCacheGuid());
    DVLOG(1) << "Creating session sync guid: " << current_machine_tag_;
    sync_prefs_->SetSyncSessionsGUID(current_machine_tag_);
  }

  local_tab_pool_.SetMachineTag(current_machine_tag_);
}

// static
void SessionsSyncManager::PopulateSessionHeaderFromSpecifics(
    const sync_pb::SessionHeader& header_specifics,
    base::Time mtime,
    SyncedSession* session_header) const {
  if (header_specifics.has_client_name())
    session_header->session_name = header_specifics.client_name();
  if (header_specifics.has_device_type()) {
    switch (header_specifics.device_type()) {
      case sync_pb::SyncEnums_DeviceType_TYPE_WIN:
        session_header->device_type = SyncedSession::TYPE_WIN;
        break;
      case sync_pb::SyncEnums_DeviceType_TYPE_MAC:
        session_header->device_type = SyncedSession::TYPE_MACOSX;
        break;
      case sync_pb::SyncEnums_DeviceType_TYPE_LINUX:
        session_header->device_type = SyncedSession::TYPE_LINUX;
        break;
      case sync_pb::SyncEnums_DeviceType_TYPE_CROS:
        session_header->device_type = SyncedSession::TYPE_CHROMEOS;
        break;
      case sync_pb::SyncEnums_DeviceType_TYPE_PHONE:
        session_header->device_type = SyncedSession::TYPE_PHONE;
        break;
      case sync_pb::SyncEnums_DeviceType_TYPE_TABLET:
        session_header->device_type = SyncedSession::TYPE_TABLET;
        break;
      case sync_pb::SyncEnums_DeviceType_TYPE_OTHER:
        // Intentionally fall-through
      default:
        session_header->device_type = SyncedSession::TYPE_OTHER;
        break;
    }
  }
  session_header->modified_time = mtime;
}

// static
void SessionsSyncManager::BuildSyncedSessionFromSpecifics(
    const std::string& session_tag,
    const sync_pb::SessionWindow& specifics,
    base::Time mtime,
    SessionWindow* session_window) {
  if (specifics.has_window_id())
    session_window->window_id.set_id(specifics.window_id());
  if (specifics.has_selected_tab_index())
    session_window->selected_tab_index = specifics.selected_tab_index();
  if (specifics.has_browser_type()) {
    if (specifics.browser_type() ==
        sync_pb::SessionWindow_BrowserType_TYPE_TABBED) {
      session_window->type = 1;
    } else {
      session_window->type = 2;
    }
  }
  session_window->timestamp = mtime;
  session_window->tabs.resize(specifics.tab_size(), NULL);
  for (int i = 0; i < specifics.tab_size(); i++) {
    SessionID::id_type tab_id = specifics.tab(i);
    session_tracker_.PutTabInWindow(session_tag,
                                    session_window->window_id.id(),
                                    tab_id,
                                    i);
  }
}

void SessionsSyncManager::RefreshFaviconVisitTimesFromForeignTab(
    const sync_pb::SessionTab& tab, const base::Time& modification_time) {
  // First go through and iterate over all the navigations, checking if any
  // have valid favicon urls.
  for (int i = 0; i < tab.navigation_size(); ++i) {
    if (!tab.navigation(i).favicon_url().empty()) {
      const std::string& page_url = tab.navigation(i).virtual_url();
      const std::string& favicon_url = tab.navigation(i).favicon_url();
      favicon_cache_.OnReceivedSyncFavicon(GURL(page_url),
                                           GURL(favicon_url),
                                           std::string(),
                                           syncer::TimeToProtoTime(
                                               modification_time));
    }
  }
}

bool SessionsSyncManager::GetSyncedFaviconForPageURL(
    const std::string& page_url,
    scoped_refptr<base::RefCountedMemory>* favicon_png) const {
  return favicon_cache_.GetSyncedFaviconForPageURL(GURL(page_url), favicon_png);
}

void SessionsSyncManager::DeleteForeignSession(
    const std::string& tag, syncer::SyncChangeList* change_output) {
 if (tag == current_machine_tag()) {
    LOG(ERROR) << "Attempting to delete local session. This is not currently "
               << "supported.";
    return;
  }

  std::set<int> tab_node_ids_to_delete;
  session_tracker_.LookupTabNodeIds(tag, &tab_node_ids_to_delete);
  if (!DisassociateForeignSession(tag)) {
    // We don't have any data for this session, our work here is done!
    return;
  }

  // Prepare deletes for the meta-node as well as individual tab nodes.
  change_output->push_back(syncer::SyncChange(
      FROM_HERE,
      SyncChange::ACTION_DELETE,
      SyncData::CreateLocalDelete(tag, syncer::SESSIONS)));

  for (std::set<int>::const_iterator it = tab_node_ids_to_delete.begin();
       it != tab_node_ids_to_delete.end();
       ++it) {
    change_output->push_back(syncer::SyncChange(
        FROM_HERE,
        SyncChange::ACTION_DELETE,
        SyncData::CreateLocalDelete(TabNodePool2::TabIdToTag(tag, *it),
                                    syncer::SESSIONS)));
  }
}

bool SessionsSyncManager::DisassociateForeignSession(
    const std::string& foreign_session_tag) {
  if (foreign_session_tag == current_machine_tag()) {
    DVLOG(1) << "Local session deleted! Doing nothing until a navigation is "
             << "triggered.";
    return false;
  }
  DVLOG(1) << "Disassociating session " << foreign_session_tag;
  return session_tracker_.DeleteSession(foreign_session_tag);
}

};  // namespace browser_sync
