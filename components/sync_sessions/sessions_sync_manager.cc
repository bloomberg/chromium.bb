// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/sessions_sync_manager.h"

#include <algorithm>
#include <utility>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/model/time.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"
#include "components/sync_sessions/tab_node_pool.h"
#include "components/variations/variations_associated_data.h"

using sessions::SerializedNavigationEntry;
using syncer::DeviceInfo;
using syncer::LocalDeviceInfoProvider;
using syncer::SyncChange;
using syncer::SyncData;

namespace sync_sessions {

namespace {

// Maximum number of favicons to sync.
// TODO(zea): pull this from the server.
const int kMaxSyncFavicons = 200;

// The maximum number of navigations in each direction we care to sync.
const int kMaxSyncNavigationCount = 6;

// The URL at which the set of synced tabs is displayed. We treat it differently
// from all other URL's as accessing it triggers a sync refresh of Sessions.
const char kNTPOpenTabSyncURL[] = "chrome://newtab/#open_tabs";

// Default number of days without activity after which a session is considered
// stale and becomes a candidate for garbage collection.
const int kDefaultStaleSessionThresholdDays = 14;  // 2 weeks.

// Comparator function for use with std::sort that will sort tabs by
// descending timestamp (i.e., most recent first).
bool TabsRecencyComparator(const sessions::SessionTab* t1,
                           const sessions::SessionTab* t2) {
  return t1->timestamp > t2->timestamp;
}

// Comparator function for use with std::sort that will sort sessions by
// descending modified_time (i.e., most recent first).
bool SessionsRecencyComparator(const SyncedSession* s1,
                               const SyncedSession* s2) {
  return s1->modified_time > s2->modified_time;
}

std::string TabNodeIdToTag(const std::string& machine_tag, int tab_node_id) {
  CHECK_GT(tab_node_id, TabNodePool::kInvalidTabNodeID) << "crbug.com/673618";
  return base::StringPrintf("%s %d", machine_tag.c_str(), tab_node_id);
}

std::string TagFromSpecifics(const sync_pb::SessionSpecifics& specifics) {
  if (specifics.has_header()) {
    return specifics.session_tag();
  } else if (specifics.has_tab()) {
    return TabNodeIdToTag(specifics.session_tag(), specifics.tab_node_id());
  } else {
    return std::string();
  }
}

sync_pb::SessionSpecifics SessionTabToSpecifics(
    const sessions::SessionTab& session_tab,
    const std::string& local_tag,
    int tab_node_id) {
  sync_pb::SessionSpecifics specifics;
  specifics.mutable_tab()->CopyFrom(session_tab.ToSyncData());
  specifics.set_session_tag(local_tag);
  specifics.set_tab_node_id(tab_node_id);
  return specifics;
}

void AppendDeletionsForTabNodes(const std::set<int>& tab_node_ids,
                                const std::string& machine_tag,
                                syncer::SyncChangeList* change_output) {
  for (std::set<int>::const_iterator it = tab_node_ids.begin();
       it != tab_node_ids.end(); ++it) {
    change_output->push_back(syncer::SyncChange(
        FROM_HERE, SyncChange::ACTION_DELETE,
        SyncData::CreateLocalDelete(TabNodeIdToTag(machine_tag, *it),
                                    syncer::SESSIONS)));
  }
}

// Ensure that the tab id is not invalid.
bool ShouldSyncTabId(SessionID::id_type tab_id) {
  if (tab_id == TabNodePool::kInvalidTabID)
    return false;
  return true;
}

}  // namespace

// |local_device| is owned by ProfileSyncService, its lifetime exceeds
// lifetime of SessionSyncManager.
SessionsSyncManager::SessionsSyncManager(
    sync_sessions::SyncSessionsClient* sessions_client,
    syncer::SyncPrefs* sync_prefs,
    LocalDeviceInfoProvider* local_device,
    std::unique_ptr<LocalSessionEventRouter> router,
    const base::Closure& sessions_updated_callback,
    const base::Closure& datatype_refresh_callback)
    : sessions_client_(sessions_client),
      session_tracker_(sessions_client),
      favicon_cache_(sessions_client->GetFaviconService(),
                     sessions_client->GetHistoryService(),
                     kMaxSyncFavicons),
      local_tab_pool_out_of_sync_(true),
      sync_prefs_(sync_prefs),
      local_device_(local_device),
      current_device_type_(sync_pb::SyncEnums_DeviceType_TYPE_OTHER),
      local_session_header_node_id_(TabNodePool::kInvalidTabNodeID),
      stale_session_threshold_days_(kDefaultStaleSessionThresholdDays),
      local_event_router_(std::move(router)),
      page_revisit_broadcaster_(this, sessions_client),
      sessions_updated_callback_(sessions_updated_callback),
      datatype_refresh_callback_(datatype_refresh_callback) {}

SessionsSyncManager::~SessionsSyncManager() {}

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
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  syncer::SyncMergeResult merge_result(type);
  DCHECK(session_tracker_.Empty());

  error_handler_ = std::move(error_handler);
  sync_processor_ = std::move(sync_processor);

  // SessionDataTypeController ensures that the local device info
  // is available before activating this datatype.
  DCHECK(local_device_);
  const DeviceInfo* local_device_info = local_device_->GetLocalDeviceInfo();
  if (!local_device_info) {
    merge_result.set_error(error_handler_->CreateAndUploadError(
        FROM_HERE, "Failed to get local device info."));
    return merge_result;
  }

  current_session_name_ = local_device_info->client_name();
  current_device_type_ = local_device_info->device_type();

  // It's possible(via RebuildAssociations) for lost_navigations_recorder_ to
  // persist between sync being stopped and started. If it did persist, it's
  // already associated with |sync_processor|, so leave it alone.
  if (!lost_navigations_recorder_.get()) {
    lost_navigations_recorder_ =
        base::MakeUnique<sync_sessions::LostNavigationsRecorder>();
    sync_processor_->AddLocalChangeObserver(lost_navigations_recorder_.get());
  }

  local_session_header_node_id_ = TabNodePool::kInvalidTabNodeID;

  // Make sure we have a machine tag.  We do this now (versus earlier) as it's
  // a conveniently safe time to assert sync is ready and the cache_guid is
  // initialized.
  if (current_machine_tag_.empty()) {
    InitializeCurrentMachineTag(local_device_->GetLocalSyncCacheGUID());
  }

  session_tracker_.SetLocalSessionTag(current_machine_tag());

  syncer::SyncChangeList new_changes;

  // First, we iterate over sync data to update our session_tracker_.
  if (!InitFromSyncModel(initial_sync_data, &new_changes)) {
    // The sync db didn't have a header node for us. Create one.
    sync_pb::EntitySpecifics specifics;
    sync_pb::SessionSpecifics* base_specifics = specifics.mutable_session();
    base_specifics->set_session_tag(current_machine_tag());
    sync_pb::SessionHeader* header_s = base_specifics->mutable_header();
    header_s->set_client_name(current_session_name_);
    header_s->set_device_type(current_device_type_);
    syncer::SyncData data = syncer::SyncData::CreateLocalData(
        current_machine_tag(), current_session_name_, specifics);
    new_changes.push_back(
        syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD, data));
  }

#if defined(OS_ANDROID)
  std::string sync_machine_tag(
      BuildMachineTag(local_device_->GetLocalSyncCacheGUID()));
  if (current_machine_tag().compare(sync_machine_tag) != 0)
    DeleteForeignSessionInternal(sync_machine_tag, &new_changes);
#endif

  // Check if anything has changed on the local client side.
  AssociateWindows(RELOAD_TABS, &new_changes);
  local_tab_pool_out_of_sync_ = false;

  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));

  local_event_router_->StartRoutingTo(this);
  return merge_result;
}

void SessionsSyncManager::AssociateWindows(
    ReloadTabsOption option,
    syncer::SyncChangeList* change_output) {
  const std::string local_tag = current_machine_tag();
  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(local_tag);
  sync_pb::SessionHeader* header_s = specifics.mutable_header();
  SyncedSession* current_session = session_tracker_.GetSession(local_tag);
  current_session->modified_time = base::Time::Now();
  header_s->set_client_name(current_session_name_);
  header_s->set_device_type(current_device_type_);

  session_tracker_.ResetSessionTracking(local_tag);
  SyncedWindowDelegatesGetter::SyncedWindowDelegateMap windows =
      synced_window_delegates_getter()->GetSyncedWindowDelegates();

  if (option == RELOAD_TABS) {
    UMA_HISTOGRAM_COUNTS("Sync.SessionWindows", windows.size());
  }
  if (windows.size() == 0) {
    // Assume that the window hasn't loaded. Attempting to associate now would
    // clobber any old windows, so just return.
    LOG(ERROR) << "No windows present, see crbug.com/639009";
    return;
  }
  for (auto window_iter_pair : windows) {
    const SyncedWindowDelegate* window_delegate = window_iter_pair.second;
    if (option == RELOAD_TABS) {
      UMA_HISTOGRAM_COUNTS("Sync.SessionTabs", window_delegate->GetTabCount());
    }

    // Make sure the window has tabs and a viewable window. The viewable window
    // check is necessary because, for example, when a browser is closed the
    // destructor is not necessarily run immediately. This means its possible
    // for us to get a handle to a browser that is about to be removed. If
    // the tab count is 0 or the window is null, the browser is about to be
    // deleted, so we ignore it.
    if (window_delegate->ShouldSync() && window_delegate->GetTabCount() &&
        window_delegate->HasWindow()) {
      sync_pb::SessionWindow window_s;
      SessionID::id_type window_id = window_delegate->GetSessionId();
      DVLOG(1) << "Associating window " << window_id << " with "
               << window_delegate->GetTabCount() << " tabs.";
      window_s.set_window_id(window_id);
      // Note: We don't bother to set selected tab index anymore. We still
      // consume it when receiving foreign sessions, as reading it is free, but
      // it triggers too many sync cycles with too little value to make setting
      // it worthwhile.
      if (window_delegate->IsTypeTabbed()) {
        window_s.set_browser_type(
            sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
      } else if (window_delegate->IsTypePopup()) {
        window_s.set_browser_type(
            sync_pb::SessionWindow_BrowserType_TYPE_POPUP);
      } else {
        // This is a custom tab within an app. These will not be restored on
        // startup if not present.
        window_s.set_browser_type(
            sync_pb::SessionWindow_BrowserType_TYPE_CUSTOM_TAB);
      }

      bool found_tabs = false;
      for (int j = 0; j < window_delegate->GetTabCount(); ++j) {
        SessionID::id_type tab_id = window_delegate->GetTabIdAt(j);
        SyncedTabDelegate* synced_tab = window_delegate->GetTabAt(j);

        // GetTabAt can return a null tab; in that case just skip it.
        if (!synced_tab)
          continue;

        if (!ShouldSyncTabId(tab_id)) {
          LOG(ERROR) << "Not syncing invalid tab with id " << tab_id;
          continue;
        }

        // Placeholder tabs are those without WebContents, either because they
        // were never loaded into memory or they were evicted from memory
        // (typically only on Android devices). They only have a tab id, window
        // id, and a saved synced id (corresponding to the tab node id). Note
        // that only placeholders have this sync id, as it's necessary to
        // properly reassociate the tab with the entity that was backing it.
        if (synced_tab->IsPlaceholderTab()) {
          // For tabs without WebContents update the |tab_id| and |window_id|,
          // as it could have changed after a session restore.
          if (synced_tab->GetSyncId() > TabNodePool::kInvalidTabNodeID) {
            AssociateRestoredPlaceholderTab(*synced_tab, tab_id, window_id,
                                            change_output);
          }
        } else if (RELOAD_TABS == option) {
          AssociateTab(synced_tab, change_output);
        }

        // If the tab was syncable, it would have been added to the tracker
        // either by the above Associate[RestoredPlaceholder]Tab call or by the
        // OnLocalTabModified method invoking AssociateTab directly. Therefore,
        // we can key whether this window has valid tabs based on the tab's
        // presence in the tracker.
        const sessions::SessionTab* tab = nullptr;
        if (session_tracker_.LookupSessionTab(local_tag, tab_id, &tab)) {
          found_tabs = true;
          window_s.add_tab(tab_id);
        }
      }
      if (found_tabs) {
        sync_pb::SessionWindow* header_window = header_s->add_window();
        *header_window = window_s;

        // Update this window's representation in the synced session tracker.
        session_tracker_.PutWindowInSession(local_tag, window_id);
        BuildSyncedSessionFromSpecifics(
            local_tag, window_s, current_session->modified_time,
            current_session->windows[window_id].get());
      }
    }
  }
  std::set<int> deleted_tab_node_ids;
  session_tracker_.CleanupLocalTabs(&deleted_tab_node_ids);
  AppendDeletionsForTabNodes(deleted_tab_node_ids, current_machine_tag(),
                             change_output);

  // Always update the header.  Sync takes care of dropping this update
  // if the entity specifics are identical (i.e windows, client name did
  // not change).
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(specifics);
  syncer::SyncData data = syncer::SyncData::CreateLocalData(
      current_machine_tag(), current_session_name_, entity);
  change_output->push_back(
      syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE, data));
}

void SessionsSyncManager::AssociateTab(SyncedTabDelegate* const tab_delegate,
                                       syncer::SyncChangeList* change_output) {
  DCHECK(!tab_delegate->IsPlaceholderTab());

  if (tab_delegate->IsBeingDestroyed()) {
    // Do nothing. By not proactively adding the tab to the session, it will be
    // removed if necessary during subsequent cleanup.
    return;
  }

  if (!tab_delegate->ShouldSync(sessions_client_))
    return;

  SessionID::id_type tab_id = tab_delegate->GetSessionId();
  DVLOG(1) << "Syncing tab " << tab_id << " from window "
           << tab_delegate->GetWindowId();

  int tab_node_id = TabNodePool::kInvalidTabNodeID;
  bool existing_tab_node =
      session_tracker_.GetTabNodeFromLocalTabId(tab_id, &tab_node_id);
  CHECK_NE(TabNodePool::kInvalidTabNodeID, tab_node_id) << "crbug.com/673618";
  tab_delegate->SetSyncId(tab_node_id);
  sessions::SessionTab* session_tab =
      session_tracker_.GetTab(current_machine_tag(), tab_id);

  // Get the previously synced url.
  int old_index = session_tab->normalized_navigation_index();
  GURL old_url;
  if (session_tab->navigations.size() > static_cast<size_t>(old_index))
    old_url = session_tab->navigations[old_index].virtual_url();

  // Update the tracker's session representation.
  SetSessionTabFromDelegate(*tab_delegate, base::Time::Now(), session_tab);
  SetVariationIds(session_tab);
  session_tracker_.GetSession(current_machine_tag())->modified_time =
      base::Time::Now();

  // Write to the sync model itself.
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_session()->CopyFrom(
      SessionTabToSpecifics(*session_tab, current_machine_tag(), tab_node_id));
  syncer::SyncData data = syncer::SyncData::CreateLocalData(
      TabNodeIdToTag(current_machine_tag(), tab_node_id), current_session_name_,
      specifics);
  change_output->push_back(
      syncer::SyncChange(FROM_HERE,
                         existing_tab_node ? syncer::SyncChange::ACTION_UPDATE
                                           : syncer::SyncChange::ACTION_ADD,
                         data));

  int current_index = tab_delegate->GetCurrentEntryIndex();
  const GURL new_url = tab_delegate->GetVirtualURLAtIndex(current_index);
  if (new_url != old_url) {
    favicon_cache_.OnFaviconVisited(
        new_url, tab_delegate->GetFaviconURLAtIndex(current_index));
    page_revisit_broadcaster_.OnPageVisit(
        new_url, tab_delegate->GetTransitionAtIndex(current_index));
  }
}

bool SessionsSyncManager::RebuildAssociations() {
  syncer::SyncDataList data(sync_processor_->GetAllSyncData(syncer::SESSIONS));
  std::unique_ptr<syncer::SyncErrorFactory> error_handler(
      std::move(error_handler_));
  std::unique_ptr<syncer::SyncChangeProcessor> processor(
      std::move(sync_processor_));

  StopSyncing(syncer::SESSIONS);
  syncer::SyncMergeResult merge_result = MergeDataAndStartSyncing(
      syncer::SESSIONS, data, std::move(processor), std::move(error_handler));
  return !merge_result.error().IsSet();
}

bool SessionsSyncManager::IsValidSessionHeader(
    const sync_pb::SessionHeader& header) {
  // Verify that tab IDs appear only once within a session.
  // Intended to prevent http://crbug.com/360822.
  std::set<int> session_tab_ids;
  for (int i = 0; i < header.window_size(); ++i) {
    const sync_pb::SessionWindow& window = header.window(i);
    for (int j = 0; j < window.tab_size(); ++j) {
      const int tab_id = window.tab(j);
      bool success = session_tab_ids.insert(tab_id).second;
      if (!success)
        return false;
    }
  }

  return true;
}

void SessionsSyncManager::OnLocalTabModified(SyncedTabDelegate* modified_tab) {
  if (!modified_tab->IsBeingDestroyed()) {
    GURL virtual_url =
      modified_tab->GetVirtualURLAtIndex(modified_tab->GetCurrentEntryIndex());
    if (virtual_url.is_valid() &&
        virtual_url.spec() == kNTPOpenTabSyncURL) {
      DVLOG(1) << "Triggering sync refresh for sessions datatype.";
      if (!datatype_refresh_callback_.is_null())
        datatype_refresh_callback_.Run();
    }
  }

  if (local_tab_pool_out_of_sync_) {
    // If our tab pool is corrupt, pay the price of a full re-association to
    // fix things up.  This takes care of the new tab modification as well.
    bool rebuild_association_succeeded = RebuildAssociations();
    DCHECK(!rebuild_association_succeeded || !local_tab_pool_out_of_sync_);
    return;
  }

  syncer::SyncChangeList changes;
  AssociateTab(modified_tab, &changes);
  // Note, we always associate windows because it's possible a tab became
  // "interesting" by going to a valid URL, in which case it needs to be added
  // to the window's tab information. Similarly, if a tab became
  // "uninteresting", we remove it from the window's tab information.
  AssociateWindows(DONT_RELOAD_TABS, &changes);
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void SessionsSyncManager::OnFaviconsChanged(const std::set<GURL>& page_urls,
                                            const GURL& /* icon_url */) {
  for (const GURL& page_url : page_urls)
    favicon_cache_.OnPageFaviconUpdated(page_url);
}

void SessionsSyncManager::StopSyncing(syncer::ModelType type) {
  local_event_router_->Stop();
  if (sync_processor_.get() && lost_navigations_recorder_.get()) {
    sync_processor_->RemoveLocalChangeObserver(
        lost_navigations_recorder_.get());
    lost_navigations_recorder_.reset();
  }
  sync_processor_.reset(nullptr);
  error_handler_.reset();
  session_tracker_.Clear();
  current_machine_tag_.clear();
  current_session_name_.clear();
  local_session_header_node_id_ = TabNodePool::kInvalidTabNodeID;
}

syncer::SyncDataList SessionsSyncManager::GetAllSyncData(
    syncer::ModelType type) const {
  syncer::SyncDataList list;
  const SyncedSession* session = nullptr;
  if (!session_tracker_.LookupLocalSession(&session))
    return syncer::SyncDataList();

  // First construct the header node.
  sync_pb::EntitySpecifics header_entity;
  header_entity.mutable_session()->set_session_tag(current_machine_tag());
  sync_pb::SessionHeader* header_specifics =
      header_entity.mutable_session()->mutable_header();
  header_specifics->MergeFrom(session->ToSessionHeader());
  syncer::SyncData data = syncer::SyncData::CreateLocalData(
      current_machine_tag(), current_session_name_, header_entity);
  list.push_back(data);

  for (auto& win_iter : session->windows) {
    for (auto& tab : win_iter.second->tabs) {
      // TODO(zea): replace with with the correct tab node id once there's a
      // sync specific wrapper for SessionTab. This method is only used in
      // tests though, so it's fine for now. crbug.com/662597
      int tab_node_id = 0;
      sync_pb::EntitySpecifics entity;
      entity.mutable_session()->CopyFrom(
          SessionTabToSpecifics(*tab, current_machine_tag(), tab_node_id));
      syncer::SyncData data = syncer::SyncData::CreateLocalData(
          TabNodeIdToTag(current_machine_tag(), tab_node_id),
          current_session_name_, entity);
      list.push_back(data);
    }
  }
  return list;
}

bool SessionsSyncManager::GetLocalSession(const SyncedSession** local_session) {
  if (current_machine_tag().empty())
    return false;
  *local_session = session_tracker_.GetSession(current_machine_tag());
  return true;
}

syncer::SyncError SessionsSyncManager::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!sync_processor_.get()) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Models not yet associated.", syncer::SESSIONS);
    return error;
  }

  for (syncer::SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end(); ++it) {
    DCHECK(it->IsValid());
    DCHECK(it->sync_data().GetSpecifics().has_session());
    const sync_pb::SessionSpecifics& session =
        it->sync_data().GetSpecifics().session();
    switch (it->change_type()) {
      case syncer::SyncChange::ACTION_DELETE:
        // Deletions are all or nothing (since we only ever delete entire
        // sessions). Therefore we don't care if it's a tab node or meta node,
        // and just ensure we've disassociated.
        if (current_machine_tag() == session.session_tag()) {
          // Another client has attempted to delete our local data (possibly by
          // error or a clock is inaccurate). Just ignore the deletion for now
          // to avoid any possible ping-pong delete/reassociate sequence, but
          // remember that this happened as our TabNodePool is inconsistent.
          local_tab_pool_out_of_sync_ = true;
          LOG(WARNING) << "Local session data deleted. Ignoring until next "
                       << "local navigation event.";
        } else if (session.has_header()) {
          // Disassociate only when header node is deleted. For tab node
          // deletions, the header node will be updated and foreign tab will
          // get deleted.
          DisassociateForeignSession(session.session_tag());
        } else if (session.has_tab()) {
          // The challenge here is that we don't know if this tab deletion is
          // being processed before or after the parent was updated to no longer
          // references the tab. Or, even more extreme, the parent has been
          // deleted as well. Tell the tracker to do what it can. The header's
          // update will mostly get us into the correct state, the only thing
          // this deletion needs to accomplish is make sure we never tell sync
          // to delete this tab later during garbage collection.
          session_tracker_.DeleteForeignTab(session.session_tag(),
                                            session.tab_node_id());
        }
        break;
      case syncer::SyncChange::ACTION_ADD:
      case syncer::SyncChange::ACTION_UPDATE:
        if (current_machine_tag() == session.session_tag()) {
          // We should only ever receive a change to our own machine's session
          // info if encryption was turned on. In that case, the data is still
          // the same, so we can ignore.
          LOG(WARNING) << "Dropping modification to local session.";
          return syncer::SyncError();
        }
        UpdateTrackerWithSpecifics(
            session, syncer::SyncDataRemote(it->sync_data()).GetModifiedTime());
        break;
      default:
        NOTREACHED() << "Processing sync changes failed, unknown change type.";
    }
  }

  if (!sessions_updated_callback_.is_null())
    sessions_updated_callback_.Run();
  return syncer::SyncError();
}

syncer::SyncChange SessionsSyncManager::TombstoneTab(
    const sync_pb::SessionSpecifics& tab) {
  if (!tab.has_tab_node_id()) {
    LOG(WARNING) << "Old sessions node without tab node id; can't tombstone.";
    return syncer::SyncChange();
  } else {
    return syncer::SyncChange(
        FROM_HERE, SyncChange::ACTION_DELETE,
        SyncData::CreateLocalDelete(
            TabNodeIdToTag(current_machine_tag(), tab.tab_node_id()),
            syncer::SESSIONS));
  }
}

bool SessionsSyncManager::GetAllForeignSessions(
    std::vector<const SyncedSession*>* sessions) {
  if (!session_tracker_.LookupAllForeignSessions(
          sessions, SyncedSessionTracker::PRESENTABLE))
    return false;
  std::sort(sessions->begin(), sessions->end(), SessionsRecencyComparator);
  return true;
}

bool SessionsSyncManager::InitFromSyncModel(
    const syncer::SyncDataList& sync_data,
    syncer::SyncChangeList* new_changes) {
  bool found_current_header = false;
  int bad_foreign_hash_count = 0;
  for (syncer::SyncDataList::const_iterator it = sync_data.begin();
       it != sync_data.end(); ++it) {
    const syncer::SyncData& data = *it;
    DCHECK(data.GetSpecifics().has_session());
    syncer::SyncDataRemote remote(data);
    const sync_pb::SessionSpecifics& specifics = data.GetSpecifics().session();
    if (specifics.session_tag().empty() ||
        (specifics.has_tab() &&
         (!specifics.has_tab_node_id() || !specifics.tab().has_tab_id()))) {
      syncer::SyncChange tombstone(TombstoneTab(specifics));
      if (tombstone.IsValid())
        new_changes->push_back(tombstone);
    } else if (specifics.session_tag() != current_machine_tag()) {
      if (TagHashFromSpecifics(specifics) == remote.GetClientTagHash()) {
        UpdateTrackerWithSpecifics(specifics, remote.GetModifiedTime());
      } else {
        // In the past, like years ago, we believe that some session data was
        // created with bad tag hashes. This causes any change this client makes
        // to that foreign data (like deletion through garbage collection) to
        // trigger a data type error because the tag looking mechanism fails. So
        // look for these and delete via remote SyncData, which uses a server id
        // lookup mechanism instead, see crbug.com/604657.
        bad_foreign_hash_count++;
        new_changes->push_back(
            syncer::SyncChange(FROM_HERE, SyncChange::ACTION_DELETE, remote));
      }
    } else {
      // This is previously stored local session information.
      if (specifics.has_header() && !found_current_header) {
        // This is our previous header node, reuse it.
        found_current_header = true;
        if (specifics.header().has_client_name())
          current_session_name_ = specifics.header().client_name();

        // TODO(zea): crbug.com/639009 update the tracker with the specifics
        // from the header node as well. This will be necessary to preserve
        // the set of open tabs when a custom tab is opened.
      } else {
        if (specifics.has_header() || !specifics.has_tab()) {
          LOG(WARNING) << "Found more than one session header node with local "
                       << "tag.";
          syncer::SyncChange tombstone(TombstoneTab(specifics));
          if (tombstone.IsValid())
            new_changes->push_back(tombstone);
        } else if (specifics.tab().tab_id() == TabNodePool::kInvalidTabID) {
          LOG(WARNING) << "Found tab node with invalid tab id.";
          syncer::SyncChange tombstone(TombstoneTab(specifics));
          if (tombstone.IsValid())
            new_changes->push_back(tombstone);
        } else {
          // This is a valid old tab node, add it to the tracker and associate
          // it.
          DVLOG(1) << "Associating local tab " << specifics.tab().tab_id()
                   << " with node " << specifics.tab_node_id();
          session_tracker_.ReassociateLocalTab(specifics.tab_node_id(),
                                               specifics.tab().tab_id());
          UpdateTrackerWithSpecifics(specifics, remote.GetModifiedTime());
        }
      }
    }
  }

  // Cleanup all foreign sessions, since orphaned tabs may have been added after
  // the header.
  std::vector<const SyncedSession*> sessions;
  session_tracker_.LookupAllForeignSessions(&sessions,
                                            SyncedSessionTracker::RAW);
  for (const auto* session : sessions) {
    session_tracker_.CleanupForeignSession(session->session_tag);
  }

  UMA_HISTOGRAM_COUNTS_100("Sync.SessionsBadForeignHashOnMergeCount",
                           bad_foreign_hash_count);

  return found_current_header;
}

void SessionsSyncManager::UpdateTrackerWithSpecifics(
    const sync_pb::SessionSpecifics& specifics,
    const base::Time& modification_time) {
  std::string session_tag = specifics.session_tag();
  SyncedSession* session = session_tracker_.GetSession(session_tag);
  if (specifics.has_header()) {
    // Read in the header data for this session. Header data is
    // essentially a collection of windows, each of which has an ordered id list
    // for their tabs.

    if (!IsValidSessionHeader(specifics.header())) {
      LOG(WARNING) << "Ignoring session node with invalid header "
                   << "and tag " << session_tag << ".";
      return;
    }

    // Load (or create) the SyncedSession object for this client.
    const sync_pb::SessionHeader& header = specifics.header();
    PopulateSessionHeaderFromSpecifics(header, modification_time, session);

    // Reset the tab/window tracking for this session (must do this before
    // we start calling PutWindowInSession and PutTabInWindow so that all
    // unused tabs/windows get cleared by the CleanupSession(...) call).
    session_tracker_.ResetSessionTracking(session_tag);

    // Process all the windows and their tab information.
    int num_windows = header.window_size();
    DVLOG(1) << "Populating " << session_tag << " with " << num_windows
             << " windows.";

    for (int i = 0; i < num_windows; ++i) {
      const sync_pb::SessionWindow& window_s = header.window(i);
      SessionID::id_type window_id = window_s.window_id();
      session_tracker_.PutWindowInSession(session_tag, window_id);
      BuildSyncedSessionFromSpecifics(session_tag, window_s, modification_time,
                                      session->windows[window_id].get());
    }
    // Delete any closed windows and unused tabs as necessary.
    session_tracker_.CleanupForeignSession(session_tag);
  } else if (specifics.has_tab()) {
    const sync_pb::SessionTab& tab_s = specifics.tab();
    SessionID::id_type tab_id = tab_s.tab_id();
    DVLOG(1) << "Populating " << session_tag << "'s tab id " << tab_id
             << " from node " << specifics.tab_node_id();

    // Ensure the tracker is aware of the tab node id. Deleting foreign sessions
    // requires deleting all relevant tab nodes, and it's easier to track the
    // tab node ids themselves separately from the tab ids.
    //
    // Note that TabIDs are not stable across restarts of a client. Consider
    // this example with two tabs:
    //
    // http://a.com  TabID1 --> NodeIDA
    // http://b.com  TabID2 --> NodeIDB
    //
    // After restart, tab ids are reallocated. e.g, one possibility:
    // http://a.com TabID2 --> NodeIDA
    // http://b.com TabID1 --> NodeIDB
    //
    // If that happened on a remote client, here we will see an update to
    // TabID1 with tab_node_id changing from NodeIDA to NodeIDB, and TabID2
    // with tab_node_id changing from NodeIDB to NodeIDA.
    //
    // We can also wind up here if we created this tab as an out-of-order
    // update to the header node for this session before actually associating
    // the tab itself, so the tab node id wasn't available at the time and
    // is currently kInvalidTabNodeID.
    //
    // In both cases, we can safely throw it into the set of node ids.
    session_tracker_.OnTabNodeSeen(session_tag, specifics.tab_node_id());
    sessions::SessionTab* tab = session_tracker_.GetTab(session_tag, tab_id);
    if (!tab->timestamp.is_null() && tab->timestamp > modification_time) {
      DVLOG(1) << "Ignoring " << session_tag << "'s session tab " << tab_id
               << " with earlier modification time: " << tab->timestamp
               << " vs " << modification_time;
      return;
    }

    // Update SessionTab based on protobuf.
    tab->SetFromSyncData(tab_s, modification_time);

    // If a favicon or favicon urls are present, load the URLs and visit
    // times into the in-memory favicon cache.
    RefreshFaviconVisitTimesFromForeignTab(tab_s, modification_time);

    // Update the last modified time.
    if (session->modified_time < modification_time)
      session->modified_time = modification_time;
  } else {
    LOG(WARNING) << "Ignoring session node with missing header/tab "
                 << "fields and tag " << session_tag << ".";
  }
}

void SessionsSyncManager::InitializeCurrentMachineTag(
    const std::string& cache_guid) {
  DCHECK(current_machine_tag_.empty());
  std::string persisted_guid;
  persisted_guid = sync_prefs_->GetSyncSessionsGUID();
  if (!persisted_guid.empty()) {
    current_machine_tag_ = persisted_guid;
    DVLOG(1) << "Restoring persisted session sync guid: " << persisted_guid;
  } else {
    DCHECK(!cache_guid.empty());
    current_machine_tag_ = BuildMachineTag(cache_guid);
    DVLOG(1) << "Creating session sync guid: " << current_machine_tag_;
    sync_prefs_->SetSyncSessionsGUID(current_machine_tag_);
  }
}

// static
void SessionsSyncManager::PopulateSessionHeaderFromSpecifics(
    const sync_pb::SessionHeader& header_specifics,
    base::Time mtime,
    SyncedSession* session_header) {
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
  session_header->modified_time =
      std::max(mtime, session_header->modified_time);
}

// static
void SessionsSyncManager::BuildSyncedSessionFromSpecifics(
    const std::string& session_tag,
    const sync_pb::SessionWindow& specifics,
    base::Time mtime,
    sessions::SessionWindow* session_window) {
  if (specifics.has_window_id())
    session_window->window_id.set_id(specifics.window_id());
  if (specifics.has_selected_tab_index())
    session_window->selected_tab_index = specifics.selected_tab_index();
  if (specifics.has_browser_type()) {
    // TODO(skuhne): Sync data writes |BrowserType| not
    // |SessionWindow::WindowType|. This should get changed.
    if (specifics.browser_type() ==
        sync_pb::SessionWindow_BrowserType_TYPE_TABBED) {
      session_window->type = sessions::SessionWindow::TYPE_TABBED;
    } else {
      // Note: custom tabs are treated like popup windows on restore, as you can
      // restore a custom tab on a platform that doesn't support them.
      session_window->type = sessions::SessionWindow::TYPE_POPUP;
    }
  }
  session_window->timestamp = mtime;
  session_window->tabs.clear();
  for (int i = 0; i < specifics.tab_size(); i++) {
    SessionID::id_type tab_id = specifics.tab(i);
    session_tracker_.PutTabInWindow(session_tag, session_window->window_id.id(),
                                    tab_id);
  }
}

void SessionsSyncManager::RefreshFaviconVisitTimesFromForeignTab(
    const sync_pb::SessionTab& tab,
    const base::Time& modification_time) {
  // First go through and iterate over all the navigations, checking if any
  // have valid favicon urls.
  for (int i = 0; i < tab.navigation_size(); ++i) {
    if (!tab.navigation(i).favicon_url().empty()) {
      const std::string& page_url = tab.navigation(i).virtual_url();
      const std::string& favicon_url = tab.navigation(i).favicon_url();
      favicon_cache_.OnReceivedSyncFavicon(
          GURL(page_url), GURL(favicon_url), std::string(),
          syncer::TimeToProtoTime(modification_time));
    }
  }
}

bool SessionsSyncManager::GetSyncedFaviconForPageURL(
    const std::string& page_url,
    scoped_refptr<base::RefCountedMemory>* favicon_png) const {
  return favicon_cache_.GetSyncedFaviconForPageURL(GURL(page_url), favicon_png);
}

void SessionsSyncManager::DeleteForeignSession(const std::string& tag) {
  syncer::SyncChangeList changes;
  DeleteForeignSessionInternal(tag, &changes);
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void SessionsSyncManager::DeleteForeignSessionInternal(
    const std::string& tag,
    syncer::SyncChangeList* change_output) {
  if (tag == current_machine_tag()) {
    LOG(ERROR) << "Attempting to delete local session. This is not currently "
               << "supported.";
    return;
  }

  std::set<int> tab_node_ids_to_delete;
  session_tracker_.LookupForeignTabNodeIds(tag, &tab_node_ids_to_delete);
  if (DisassociateForeignSession(tag)) {
    // Only tell sync to delete the header if there was one.
    change_output->push_back(
        syncer::SyncChange(FROM_HERE, SyncChange::ACTION_DELETE,
                           SyncData::CreateLocalDelete(tag, syncer::SESSIONS)));
  }
  AppendDeletionsForTabNodes(tab_node_ids_to_delete, tag, change_output);
  if (!sessions_updated_callback_.is_null())
    sessions_updated_callback_.Run();
}

bool SessionsSyncManager::DisassociateForeignSession(
    const std::string& foreign_session_tag) {
  DCHECK_NE(foreign_session_tag, current_machine_tag());
  DVLOG(1) << "Disassociating session " << foreign_session_tag;
  return session_tracker_.DeleteForeignSession(foreign_session_tag);
}

bool SessionsSyncManager::GetForeignSession(
    const std::string& tag,
    std::vector<const sessions::SessionWindow*>* windows) {
  return session_tracker_.LookupSessionWindows(tag, windows);
}

bool SessionsSyncManager::GetForeignSessionTabs(
    const std::string& tag,
    std::vector<const sessions::SessionTab*>* tabs) {
  std::vector<const sessions::SessionWindow*> windows;
  if (!session_tracker_.LookupSessionWindows(tag, &windows))
    return false;

  // Prune those tabs that are not syncable or are NewTabPage, then sort them
  // from most recent to least recent, independent of which window the tabs were
  // from.
  for (size_t j = 0; j < windows.size(); ++j) {
    const sessions::SessionWindow* window = windows[j];
    for (size_t t = 0; t < window->tabs.size(); ++t) {
      sessions::SessionTab* const tab = window->tabs[t].get();
      if (tab->navigations.empty())
        continue;
      const sessions::SerializedNavigationEntry& current_navigation =
          tab->navigations.at(tab->normalized_navigation_index());
      if (!sessions_client_->ShouldSyncURL(current_navigation.virtual_url()))
        continue;
      tabs->push_back(tab);
    }
  }
  std::sort(tabs->begin(), tabs->end(), TabsRecencyComparator);
  return true;
}

bool SessionsSyncManager::GetForeignTab(const std::string& tag,
                                        const SessionID::id_type tab_id,
                                        const sessions::SessionTab** tab) {
  const sessions::SessionTab* synced_tab = nullptr;
  bool success = session_tracker_.LookupSessionTab(tag, tab_id, &synced_tab);
  if (success)
    *tab = synced_tab;
  return success;
}

void SessionsSyncManager::AssociateRestoredPlaceholderTab(
    const SyncedTabDelegate& tab_delegate,
    SessionID::id_type new_tab_id,
    SessionID::id_type new_window_id,
    syncer::SyncChangeList* change_output) {
  DCHECK_NE(tab_delegate.GetSyncId(), TabNodePool::kInvalidTabNodeID);

  // It's possible the placeholder tab is associated with a tab node that's
  // since been deleted. If that's the case, there's no way to reassociate it,
  // so just return now without adding the tab to the session tracker.
  if (!session_tracker_.IsLocalTabNodeAssociated(tab_delegate.GetSyncId())) {
    DVLOG(1) << "Restored placeholder tab's node " << tab_delegate.GetSyncId()
             << " deleted.";
    return;
  }

  // Update tracker with the new association (and inform it of the tab node
  // in the process).
  session_tracker_.ReassociateLocalTab(tab_delegate.GetSyncId(), new_tab_id);

  // Update the window id on the SessionTab itself.
  sessions::SessionTab* local_tab =
      session_tracker_.GetTab(current_machine_tag(), new_tab_id);
  local_tab->window_id.set_id(new_window_id);

  // Rewrite the specifics based on the reassociated SessionTab to preserve
  // the new tab and window ids.
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(SessionTabToSpecifics(
      *local_tab, current_machine_tag(), tab_delegate.GetSyncId()));
  syncer::SyncData data = syncer::SyncData::CreateLocalData(
      TabNodeIdToTag(current_machine_tag(), tab_delegate.GetSyncId()),
      current_session_name_, entity);
  change_output->push_back(
      syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE, data));
}

// static
void SessionsSyncManager::SetSessionTabFromDelegate(
    const SyncedTabDelegate& tab_delegate,
    base::Time mtime,
    sessions::SessionTab* session_tab) {
  DCHECK(session_tab);
  session_tab->window_id.set_id(tab_delegate.GetWindowId());
  session_tab->tab_id.set_id(tab_delegate.GetSessionId());
  session_tab->tab_visual_index = 0;
  // Use -1 to indicate that the index hasn't been set properly yet.
  session_tab->current_navigation_index = -1;
  const SyncedWindowDelegate* window_delegate =
      synced_window_delegates_getter()->FindById(tab_delegate.GetWindowId());
  session_tab->pinned =
      window_delegate ? window_delegate->IsTabPinned(&tab_delegate) : false;
  session_tab->extension_app_id = tab_delegate.GetExtensionAppId();
  session_tab->user_agent_override.clear();
  session_tab->timestamp = mtime;
  const int current_index = tab_delegate.GetCurrentEntryIndex();
  const int min_index = std::max(0, current_index - kMaxSyncNavigationCount);
  const int max_index = std::min(current_index + kMaxSyncNavigationCount,
                                 tab_delegate.GetEntryCount());
  bool is_supervised = tab_delegate.ProfileIsSupervised();
  session_tab->navigations.clear();

  for (int i = min_index; i < max_index; ++i) {
    if (!tab_delegate.GetVirtualURLAtIndex(i).is_valid())
      continue;
    sessions::SerializedNavigationEntry serialized_entry;
    tab_delegate.GetSerializedNavigationAtIndex(i, &serialized_entry);

    // Set current_navigation_index to the index in navigations.
    if (i == current_index)
      session_tab->current_navigation_index = session_tab->navigations.size();

    session_tab->navigations.push_back(serialized_entry);
    if (is_supervised) {
      session_tab->navigations.back().set_blocked_state(
          SerializedNavigationEntry::STATE_ALLOWED);
    }
  }

  // If the current navigation is invalid, set the index to the end of the
  // navigation array.
  if (session_tab->current_navigation_index < 0) {
    session_tab->current_navigation_index = session_tab->navigations.size() - 1;
  }

  if (is_supervised) {
    int offset = session_tab->navigations.size();
    const std::vector<std::unique_ptr<const SerializedNavigationEntry>>&
        blocked_navigations = *tab_delegate.GetBlockedNavigations();
    for (size_t i = 0; i < blocked_navigations.size(); ++i) {
      session_tab->navigations.push_back(*blocked_navigations[i]);
      session_tab->navigations.back().set_index(offset + i);
      session_tab->navigations.back().set_blocked_state(
          SerializedNavigationEntry::STATE_BLOCKED);
      // TODO(bauerb): Add categories
    }
  }
  session_tab->session_storage_persistent_id.clear();
}

// static
void SessionsSyncManager::SetVariationIds(sessions::SessionTab* session_tab) {
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  for (const base::FieldTrial::ActiveGroup& group : active_groups) {
    const variations::VariationID id = variations::GetGoogleVariationID(
        variations::CHROME_SYNC_SERVICE, group.trial_name, group.group_name);
    if (id != variations::EMPTY_ID)
      session_tab->variation_ids.push_back(id);
  }
}

FaviconCache* SessionsSyncManager::GetFaviconCache() {
  return &favicon_cache_;
}

SyncedWindowDelegatesGetter*
SessionsSyncManager::synced_window_delegates_getter() const {
  return sessions_client_->GetSyncedWindowDelegatesGetter();
}

void SessionsSyncManager::DoGarbageCollection() {
  std::vector<const SyncedSession*> sessions;
  if (!session_tracker_.LookupAllForeignSessions(&sessions,
                                                 SyncedSessionTracker::RAW))
    return;  // No foreign sessions.

  // Iterate through all the sessions and delete any with age older than
  // |stale_session_threshold_days_|.
  syncer::SyncChangeList changes;
  for (const auto* session : sessions) {
    int session_age_in_days =
        (base::Time::Now() - session->modified_time).InDays();
    if (session_age_in_days > stale_session_threshold_days_) {
      std::string session_tag = session->session_tag;
      DVLOG(1) << "Found stale session " << session_tag << " with age "
               << session_age_in_days << ", deleting.";
      DeleteForeignSessionInternal(session_tag, &changes);
    }
  }

  if (!changes.empty())
    sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

// static
std::string SessionsSyncManager::TagHashFromSpecifics(
    const sync_pb::SessionSpecifics& specifics) {
  return syncer::GenerateSyncableHash(syncer::SESSIONS,
                                      TagFromSpecifics(specifics));
}

};  // namespace sync_sessions
