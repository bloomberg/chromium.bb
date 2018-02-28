// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/sessions_sync_manager.h"

#include <algorithm>
#include <utility>

#include "base/format_macros.h"
#include "base/logging.h"
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

// Default number of days without activity after which a session is considered
// stale and becomes a candidate for garbage collection.
const int kDefaultStaleSessionThresholdDays = 14;  // 2 weeks.

std::string TabNodeIdToTag(const std::string& machine_tag, int tab_node_id) {
  CHECK_GT(tab_node_id, TabNodePool::kInvalidTabNodeID)
      << "https://crbug.com/639009";
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
  if (tab_id == kInvalidTabID)
    return false;
  return true;
}

bool IsWindowSyncable(const SyncedWindowDelegate& window_delegate) {
  return window_delegate.ShouldSync() && window_delegate.GetTabCount() &&
         window_delegate.HasWindow();
}

}  // namespace

// |local_device| is owned by ProfileSyncService, its lifetime exceeds
// lifetime of SessionSyncManager.
SessionsSyncManager::SessionsSyncManager(
    sync_sessions::SyncSessionsClient* sessions_client,
    syncer::SyncPrefs* sync_prefs,
    LocalDeviceInfoProvider* local_device,
    LocalSessionEventRouter* router,
    const base::RepeatingClosure& sessions_updated_callback)
    : sessions_client_(sessions_client),
      session_tracker_(sessions_client),
      favicon_cache_(sessions_client->GetFaviconService(),
                     sessions_client->GetHistoryService(),
                     kMaxSyncFavicons),
      open_tabs_ui_delegate_(
          sessions_client,
          &session_tracker_,
          &favicon_cache_,
          base::BindRepeating(&SessionsSyncManager::DeleteForeignSessionFromUI,
                              base::Unretained(this))),
      local_tab_pool_out_of_sync_(true),
      sync_prefs_(sync_prefs),
      local_device_(local_device),
      current_device_type_(sync_pb::SyncEnums_DeviceType_TYPE_OTHER),
      local_session_header_node_id_(TabNodePool::kInvalidTabNodeID),
      stale_session_threshold_days_(kDefaultStaleSessionThresholdDays),
      local_event_router_(router),
      sessions_updated_callback_(sessions_updated_callback),
      task_tracker_(std::make_unique<TaskTracker>()) {}

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
        std::make_unique<sync_sessions::LostNavigationsRecorder>();
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
  AssociateWindows(RELOAD_TABS, ScanForTabbedWindow(), &new_changes);
  local_tab_pool_out_of_sync_ = false;

  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));

  local_event_router_->StartRoutingTo(this);
  return merge_result;
}

void SessionsSyncManager::AssociateWindows(
    ReloadTabsOption option,
    bool has_tabbed_window,
    syncer::SyncChangeList* change_output) {
  // Note that |current_session| is a pointer owned by |session_tracker_|.
  // |session_tracker_| will continue to update |current_session| under
  // the hood so care must be taken accessing it. In particular, invoking
  // ResetSessionTracking(..) will invalidate all the tab data within
  // the session, hence why copies of the SyncedSession must be made ahead of
  // time.
  SyncedSession* current_session =
      session_tracker_.GetSession(current_machine_tag());
  current_session->session_name = current_session_name_;
  current_session->device_type = current_device_type_;
  current_session->session_tag = current_machine_tag();

  SyncedWindowDelegatesGetter::SyncedWindowDelegateMap windows =
      synced_window_delegates_getter()->GetSyncedWindowDelegates();

  // Without native data, we need be careful not to obliterate any old
  // information, while at the same time handling updated tab ids. See
  // https://crbug.com/639009 for more info.
  if (has_tabbed_window) {
    // Just reset the session tracking. No need to worry about the previous
    // session; the current tabbed windows are now the source of truth.
    session_tracker_.ResetSessionTracking(current_machine_tag());
    current_session->modified_time = base::Time::Now();
  } else {
    DVLOG(1) << "Found no tabbed windows. Reloading "
             << current_session->windows.size()
             << " windows from previous session.";

    // A copy of the specifics must be made because |current_session| will be
    // updated in place and therefore can't be relied on as the source of truth.
    sync_pb::SessionSpecifics specifics;
    specifics.set_session_tag(current_machine_tag());
    specifics.mutable_header()->CopyFrom(
        current_session->ToSessionHeaderProto());
    UpdateTrackerWithSpecifics(specifics, base::Time::Now(), &session_tracker_);

    // The tab entities stored in sync have outdated SessionId values. Go
    // through and update them to the new SessionIds.
    for (auto& win_iter : current_session->windows) {
      for (auto& tab : win_iter.second->wrapped_window.tabs) {
        int sync_id = TabNodePool::kInvalidTabNodeID;
        if (!session_tracker_.GetTabNodeFromLocalTabId(tab->tab_id.id(),
                                                       &sync_id) ||
            sync_id == TabNodePool::kInvalidTabNodeID) {
          continue;
        }
        DVLOG(1) << "Rewriting tab node " << sync_id << " with tab id "
                 << tab->tab_id.id();
        AppendChangeForExistingTab(sync_id, *tab, change_output);
      }
    }
  }

  // Each sync id should only ever be used once. Previously there existed a race
  // condition which could cause them to be duplicated, see
  // https://crbug.com/639009 for more information. This counts the number of
  // times each id is used so that the second window/tab loop can act on every
  // tab using duplicate ids. Lastly, it is important to note that this
  // duplication scan is only checking the in-memory tab state. On Android, if
  // we have no tabbed window, we may also have sync data with conflicting sync
  // ids, but to keep this logic simple and less error prone, we do not attempt
  // to do anything clever.
  std::map<int, size_t> sync_id_count;
  int duplicate_count = 0;
  for (auto& window_iter_pair : windows) {
    const SyncedWindowDelegate* window_delegate = window_iter_pair.second;
    if (IsWindowSyncable(*window_delegate)) {
      for (int j = 0; j < window_delegate->GetTabCount(); ++j) {
        SyncedTabDelegate* synced_tab = window_delegate->GetTabAt(j);
        if (synced_tab &&
            synced_tab->GetSyncId() != TabNodePool::kInvalidTabNodeID) {
          auto iter = sync_id_count.find(synced_tab->GetSyncId());
          if (iter == sync_id_count.end()) {
            sync_id_count.insert(iter,
                                 std::make_pair(synced_tab->GetSyncId(), 1));
          } else {
            // If an id is used more than twice, this count will be a bit odd,
            // but for our purposes, it will be sufficient.
            duplicate_count++;
            iter->second++;
          }
        }
      }
    }
  }
  if (duplicate_count > 0) {
    UMA_HISTOGRAM_COUNTS_100("Sync.SesssionsDuplicateSyncId", duplicate_count);
  }

  for (auto& window_iter_pair : windows) {
    const SyncedWindowDelegate* window_delegate = window_iter_pair.second;
    if (option == RELOAD_TABS) {
      UMA_HISTOGRAM_COUNTS("Sync.SessionTabs", window_delegate->GetTabCount());
    }

    // Make sure the window has tabs and a viewable window. The viewable
    // window check is necessary because, for example, when a browser is
    // closed the destructor is not necessarily run immediately. This means
    // its possible for us to get a handle to a browser that is about to be
    // removed. If the tab count is 0 or the window is null, the browser is
    // about to be deleted, so we ignore it.
    if (IsWindowSyncable(*window_delegate)) {
      SessionID::id_type window_id = window_delegate->GetSessionId();
      DVLOG(1) << "Associating window " << window_id << " with "
               << window_delegate->GetTabCount() << " tabs.";

      bool found_tabs = false;
      for (int j = 0; j < window_delegate->GetTabCount(); ++j) {
        SessionID::id_type tab_id = window_delegate->GetTabIdAt(j);
        SyncedTabDelegate* synced_tab = window_delegate->GetTabAt(j);

        // GetTabAt can return a null tab; in that case just skip it. Similarly,
        // if for some reason the tab id is invalid, skip it.
        if (!synced_tab || !ShouldSyncTabId(tab_id))
          continue;

        if (synced_tab->GetSyncId() != TabNodePool::kInvalidTabNodeID) {
          auto duplicate_iter = sync_id_count.find(synced_tab->GetSyncId());
          DCHECK(duplicate_iter != sync_id_count.end());
          if (duplicate_iter->second > 1) {
            // Strip the id before processing it. This is going to mean it'll be
            // treated the same as a new tab. If it's also a placeholder, we'll
            // have no data about it, sync it cannot be synced until it is
            // loaded. It is too difficult to try to guess which of the multiple
            // tabs using the same id actually corresponds to the existing sync
            // data.
            synced_tab->SetSyncId(TabNodePool::kInvalidTabNodeID);
          }
        }

        // Placeholder tabs are those without WebContents, either because they
        // were never loaded into memory or they were evicted from memory
        // (typically only on Android devices). They only have a tab id,
        // window id, and a saved synced id (corresponding to the tab node
        // id). Note that only placeholders have this sync id, as it's
        // necessary to properly reassociate the tab with the entity that was
        // backing it.
        if (synced_tab->IsPlaceholderTab()) {
          // For tabs without WebContents update the |tab_id| and |window_id|,
          // as it could have changed after a session restore.
          if (synced_tab->GetSyncId() > TabNodePool::kInvalidTabNodeID) {
            AssociateRestoredPlaceholderTab(*synced_tab, tab_id, window_id,
                                            change_output);
          } else {
            DVLOG(1) << "Placeholder tab " << tab_id << " has no sync id.";
          }
        } else if (RELOAD_TABS == option) {
          AssociateTab(synced_tab, has_tabbed_window, change_output);
        }

        // If the tab was syncable, it would have been added to the tracker
        // either by the above Associate[RestoredPlaceholder]Tab call or by
        // the OnLocalTabModified method invoking AssociateTab directly.
        // Therefore, we can key whether this window has valid tabs based on
        // the tab's presence in the tracker.
        const sessions::SessionTab* tab = nullptr;
        if (session_tracker_.LookupSessionTab(current_machine_tag(), tab_id,
                                              &tab)) {
          found_tabs = true;

          // Update this window's representation in the synced session tracker.
          // This is a no-op if called multiple times.
          session_tracker_.PutWindowInSession(current_machine_tag(), window_id);

          // Put the tab in the window (must happen after the window is added
          // to the session).
          session_tracker_.PutTabInWindow(current_machine_tag(), window_id,
                                          tab_id);
        }
      }
      if (found_tabs) {
        SyncedSessionWindow* synced_session_window =
            current_session->windows[window_id].get();
        if (window_delegate->IsTypeTabbed()) {
          synced_session_window->window_type =
              sync_pb::SessionWindow_BrowserType_TYPE_TABBED;
        } else if (window_delegate->IsTypePopup()) {
          synced_session_window->window_type =
              sync_pb::SessionWindow_BrowserType_TYPE_POPUP;
        } else {
          // This is a custom tab within an app. These will not be restored on
          // startup if not present.
          synced_session_window->window_type =
              sync_pb::SessionWindow_BrowserType_TYPE_CUSTOM_TAB;
        }
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
  entity.mutable_session()->set_session_tag(current_machine_tag());
  entity.mutable_session()->mutable_header()->CopyFrom(
      current_session->ToSessionHeaderProto());
  syncer::SyncData data = syncer::SyncData::CreateLocalData(
      current_machine_tag(), current_session_name_, entity);
  change_output->push_back(
      syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE, data));
}

void SessionsSyncManager::AssociateTab(SyncedTabDelegate* const tab_delegate,
                                       bool has_tabbed_window,
                                       syncer::SyncChangeList* change_output) {
  DCHECK(!tab_delegate->IsPlaceholderTab());

  if (tab_delegate->IsBeingDestroyed()) {
    task_tracker_->CleanTabTasks(tab_delegate->GetSessionId());
    // Do nothing else. By not proactively adding the tab to the session, it
    // will be removed if necessary during subsequent cleanup.
    return;
  }

  // Ensure the task tracker has up to date task ids for this tab.
  UpdateTaskTracker(tab_delegate);

  if (!tab_delegate->ShouldSync(sessions_client_))
    return;

  SessionID::id_type tab_id = tab_delegate->GetSessionId();
  DVLOG(1) << "Syncing tab " << tab_id << " from window "
           << tab_delegate->GetWindowId();

  int tab_node_id = TabNodePool::kInvalidTabNodeID;
  bool existing_tab_node = true;
  if (session_tracker_.IsLocalTabNodeAssociated(tab_delegate->GetSyncId())) {
    tab_node_id = tab_delegate->GetSyncId();
    session_tracker_.ReassociateLocalTab(tab_node_id, tab_id);
  } else if (has_tabbed_window) {
    existing_tab_node =
        session_tracker_.GetTabNodeFromLocalTabId(tab_id, &tab_node_id);
    CHECK_NE(TabNodePool::kInvalidTabNodeID, tab_node_id)
        << "https://crbug.com/639009";
    tab_delegate->SetSyncId(tab_node_id);
  } else {
    // Only allowed to allocate sync ids when we have native data, which is only
    // true when we have a tabbed window. Without a sync id we cannot sync this
    // data, the tracker cannot even really track it. So don't do any more work.
    // This effectively breaks syncing custom tabs when the native browser isn't
    // fully loaded. Ideally this is fixed by saving tab data and sync data
    // atomically, see https://crbug.com/681921.
    return;
  }

  sessions::SessionTab* session_tab =
      session_tracker_.GetTab(current_machine_tag(), tab_id);

  // Get the previously synced url.
  int old_index = session_tab->normalized_navigation_index();
  GURL old_url;
  if (session_tab->navigations.size() > static_cast<size_t>(old_index))
    old_url = session_tab->navigations[old_index].virtual_url();

  // Update the tracker's session representation.
  SetSessionTabFromDelegate(*tab_delegate, base::Time::Now(), session_tab);
  session_tracker_.GetSession(current_machine_tag())->modified_time =
      base::Time::Now();

  // Write to the sync model itself.
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_session()->CopyFrom(
      SessionTabToSpecifics(*session_tab, current_machine_tag(), tab_node_id));
  WriteTasksIntoSpecifics(specifics.mutable_session()->mutable_tab());
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
  }
}

void SessionsSyncManager::UpdateTaskTracker(
    SyncedTabDelegate* const tab_delegate) {
  TabTasks* tab_tasks = task_tracker_->GetTabTasks(
      tab_delegate->GetSessionId(), tab_delegate->GetSourceTabID());

  // Iterate through all navigations in the tab to ensure they all have a task
  // id set (it's possible some haven't been seen before, such as when a tab
  // is restored).
  for (int i = 0; i < tab_delegate->GetEntryCount(); ++i) {
    sessions::SerializedNavigationEntry serialized_entry;
    tab_delegate->GetSerializedNavigationAtIndex(i, &serialized_entry);

    int nav_id = serialized_entry.unique_id();
    int64_t global_id = serialized_entry.timestamp().ToInternalValue();
    tab_tasks->UpdateWithNavigation(
        nav_id, tab_delegate->GetTransitionAtIndex(i), global_id);
  }
}

void SessionsSyncManager::WriteTasksIntoSpecifics(
    sync_pb::SessionTab* tab_specifics) {
  TabTasks* tab_tasks =
      task_tracker_->GetTabTasks(tab_specifics->tab_id(), kInvalidTabID);
  for (int i = 0; i < tab_specifics->navigation_size(); i++) {
    // Excluding blocked navigations, which are appended at tail.
    if (tab_specifics->navigation(i).blocked_state() ==
        sync_pb::TabNavigation::STATE_BLOCKED) {
      break;
    }

    std::vector<int64_t> task_ids = tab_tasks->GetTaskIdsForNavigation(
        tab_specifics->navigation(i).unique_id());
    if (task_ids.empty())
      continue;

    tab_specifics->mutable_navigation(i)->set_task_id(task_ids.back());
    // Pop the task id of navigation self.
    task_ids.pop_back();
    tab_specifics->mutable_navigation(i)->clear_ancestor_task_id();
    for (auto ancestor_task_id : task_ids) {
      tab_specifics->mutable_navigation(i)->add_ancestor_task_id(
          ancestor_task_id);
    }
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

void SessionsSyncManager::OnLocalTabModified(SyncedTabDelegate* modified_tab) {
  sessions::SerializedNavigationEntry current;
  modified_tab->GetSerializedNavigationAtIndex(
      modified_tab->GetCurrentEntryIndex(), &current);
  global_id_mapper_.TrackNavigationIds(current.timestamp(),
                                       current.unique_id());

  if (local_tab_pool_out_of_sync_) {
    // If our tab pool is corrupt, pay the price of a full re-association to
    // fix things up.  This takes care of the new tab modification as well.
    bool rebuild_association_succeeded = RebuildAssociations();
    DCHECK(!rebuild_association_succeeded || !local_tab_pool_out_of_sync_);
    return;
  }

  bool found_tabbed_window = ScanForTabbedWindow();
  syncer::SyncChangeList changes;
  AssociateTab(modified_tab, found_tabbed_window, &changes);
  // Note, we always associate windows because it's possible a tab became
  // "interesting" by going to a valid URL, in which case it needs to be added
  // to the window's tab information. Similarly, if a tab became
  // "uninteresting", we remove it from the window's tab information.
  AssociateWindows(DONT_RELOAD_TABS, found_tabbed_window, &changes);
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void SessionsSyncManager::OnFaviconsChanged(const std::set<GURL>& page_urls,
                                            const GURL& /* icon_url */) {
  for (const GURL& page_url : page_urls) {
    if (page_url.is_valid())
      favicon_cache_.OnPageFaviconUpdated(page_url);
  }
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
  header_specifics->MergeFrom(session->ToSessionHeaderProto());
  syncer::SyncData data = syncer::SyncData::CreateLocalData(
      current_machine_tag(), current_session_name_, header_entity);
  list.push_back(data);

  for (auto& win_iter : session->windows) {
    for (auto& tab : win_iter.second->wrapped_window.tabs) {
      // TODO(zea): replace with with the correct tab node id once there's a
      // sync specific wrapper for SessionTab. This method is only used in
      // tests though, so it's fine for now. https://crbug.com/662597
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

syncer::SyncError SessionsSyncManager::ProcessSyncChanges(
    const base::Location& from_here,
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
    const base::Time mtime =
        syncer::SyncDataRemote(it->sync_data()).GetModifiedTime();
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
        UpdateTrackerWithSpecifics(session, mtime, &session_tracker_);
        // If a favicon or favicon urls are present, load the URLs and visit
        // times into the in-memory favicon cache.
        if (session.has_tab()) {
          RefreshFaviconVisitTimesFromForeignTab(session.tab(), mtime);
        }
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

bool SessionsSyncManager::InitFromSyncModel(
    const syncer::SyncDataList& sync_data,
    syncer::SyncChangeList* new_changes) {
  // Map of all rewritten local ids. Because ids are reset on each restart,
  // and id generation happens outside of Sync, all ids from a previous local
  // session must be rewritten in order to be valid.
  // Key: previous session id. Value: new session id.
  std::map<SessionID::id_type, SessionID::id_type> session_id_map;

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
        UpdateTrackerWithSpecifics(specifics, remote.GetModifiedTime(),
                                   &session_tracker_);
        // If a favicon or favicon urls are present, load the URLs and visit
        // times into the in-memory favicon cache.
        if (specifics.has_tab()) {
          RefreshFaviconVisitTimesFromForeignTab(specifics.tab(),
                                                 remote.GetModifiedTime());
        }
      } else {
        // In the past, like years ago, we believe that some session data was
        // created with bad tag hashes. This causes any change this client makes
        // to that foreign data (like deletion through garbage collection) to
        // trigger a data type error because the tag looking mechanism fails. So
        // look for these and delete via remote SyncData, which uses a server id
        // lookup mechanism instead, see https://crbug.com/604657.
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

        // The specifics from the SyncData are immutable. Create a mutable copy
        // to hold the rewritten ids.
        sync_pb::SessionSpecifics rewritten_specifics(specifics);

        // Go through and generate new tab and window ids as necessary, updating
        // the specifics in place.
        for (auto& window :
             *rewritten_specifics.mutable_header()->mutable_window()) {
          session_id_map[window.window_id()] = SessionID().id();
          window.set_window_id(session_id_map[window.window_id()]);

          google::protobuf::RepeatedField<int>* tab_ids = window.mutable_tab();
          for (int i = 0; i < tab_ids->size(); i++) {
            auto tab_iter = session_id_map.find(tab_ids->Get(i));
            if (tab_iter == session_id_map.end()) {
              // SessionID::SessionID() automatically increments a static
              // variable, forcing a new id to be generated each time.
              session_id_map[tab_ids->Get(i)] = SessionID().id();
            }
            *(tab_ids->Mutable(i)) = session_id_map[tab_ids->Get(i)];
            // Note: the tab id of the SessionTab will be updated when the tab
            // node itself is processed.
          }
        }

        UpdateTrackerWithSpecifics(rewritten_specifics,
                                   remote.GetModifiedTime(), &session_tracker_);

        DVLOG(1) << "Loaded local header and rewrote " << session_id_map.size()
                 << " ids.";

      } else {
        if (specifics.has_header() || !specifics.has_tab()) {
          LOG(WARNING) << "Found more than one session header node with local "
                       << "tag.";
          syncer::SyncChange tombstone(TombstoneTab(specifics));
          if (tombstone.IsValid())
            new_changes->push_back(tombstone);
        } else if (specifics.tab().tab_id() == kInvalidTabID) {
          LOG(WARNING) << "Found tab node with invalid tab id.";
          syncer::SyncChange tombstone(TombstoneTab(specifics));
          if (tombstone.IsValid())
            new_changes->push_back(tombstone);
        } else {
          // This is a valid old tab node, add it to the tracker and associate
          // it (using the new tab id).
          DVLOG(1) << "Associating local tab " << specifics.tab().tab_id()
                   << " with node " << specifics.tab_node_id();

          // Now file the tab under the new tab id.
          SessionID::id_type new_tab_id = kInvalidTabID;
          auto iter = session_id_map.find(specifics.tab().tab_id());
          if (iter != session_id_map.end()) {
            new_tab_id = iter->second;
          } else {
            session_id_map[specifics.tab().tab_id()] = SessionID().id();
            new_tab_id = session_id_map[specifics.tab().tab_id()];
          }
          DVLOG(1) << "Remapping tab " << specifics.tab().tab_id() << " to "
                   << new_tab_id;

          // The specifics from the SyncData are immutable. Create a mutable
          // copy to hold the rewritten ids.
          sync_pb::SessionSpecifics rewritten_specifics(specifics);
          rewritten_specifics.mutable_tab()->set_tab_id(new_tab_id);
          session_tracker_.ReassociateLocalTab(
              rewritten_specifics.tab_node_id(), new_tab_id);
          UpdateTrackerWithSpecifics(
              rewritten_specifics, remote.GetModifiedTime(), &session_tracker_);
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
    session_tracker_.CleanupSession(session->session_tag);
  }

  UMA_HISTOGRAM_COUNTS_100("Sync.SessionsBadForeignHashOnMergeCount",
                           bad_foreign_hash_count);

  return found_current_header;
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

void SessionsSyncManager::DeleteForeignSessionFromUI(const std::string& tag) {
  syncer::SyncChangeList changes;
  DeleteForeignSessionInternal(tag, &changes);
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

bool SessionsSyncManager::DisassociateForeignSession(
    const std::string& foreign_session_tag) {
  DCHECK_NE(foreign_session_tag, current_machine_tag());
  DVLOG(1) << "Disassociating session " << foreign_session_tag;
  return session_tracker_.DeleteForeignSession(foreign_session_tag);
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

  AppendChangeForExistingTab(tab_delegate.GetSyncId(), *local_tab,
                             change_output);
}

void SessionsSyncManager::AppendChangeForExistingTab(
    int sync_id,
    const sessions::SessionTab& tab,
    syncer::SyncChangeList* change_output) {
  // Rewrite the specifics based on the reassociated SessionTab to preserve
  // the new tab and window ids.
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(
      SessionTabToSpecifics(tab, current_machine_tag(), sync_id));
  syncer::SyncData data = syncer::SyncData::CreateLocalData(
      TabNodeIdToTag(current_machine_tag(), sync_id), current_session_name_,
      entity);
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

FaviconCache* SessionsSyncManager::GetFaviconCache() {
  return &favicon_cache_;
}

SessionsGlobalIdMapper* SessionsSyncManager::GetGlobalIdMapper() {
  return &global_id_mapper_;
}

OpenTabsUIDelegate* SessionsSyncManager::GetOpenTabsUIDelegate() {
  return &open_tabs_ui_delegate_;
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

bool SessionsSyncManager::ScanForTabbedWindow() {
  for (auto& window_iter_pair :
       synced_window_delegates_getter()->GetSyncedWindowDelegates()) {
    if (window_iter_pair.second->IsTypeTabbed()) {
      const SyncedWindowDelegate* window_delegate = window_iter_pair.second;
      if (IsWindowSyncable(*window_delegate)) {
        // When only custom tab windows are open, often we'll have a seemingly
        // okay type tabbed window, but GetTabAt will return null for each
        // index. This case is exactly what this method needs to protect
        // against.
        for (int j = 0; j < window_delegate->GetTabCount(); ++j) {
          if (window_delegate->GetTabAt(j))
            return true;
        }
      }
    }
  }
  return false;
}

}  // namespace sync_sessions
