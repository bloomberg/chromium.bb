// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/session_model_associator.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_state_map.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/read_transaction.h"
#include "sync/syncable/write_transaction.h"
#include "sync/util/get_session_name.h"
#include "sync/util/time.h"
#include "ui/gfx/favicon_size.h"
#if defined(OS_LINUX)
#include "base/linux_util.h"
#elif defined(OS_WIN)
#include <windows.h>
#elif defined(OS_ANDROID)
#include "sync/util/session_utils_android.h"
#endif

using content::BrowserThread;
using content::NavigationEntry;
using prefs::kSyncSessionsGUID;
using syncer::SESSIONS;

namespace browser_sync {

namespace {
static const char kNoSessionsFolderError[] =
    "Server did not create the top-level sessions node. We "
    "might be running against an out-of-date server.";

// The maximum number of navigations in each direction we care to sync.
static const int kMaxSyncNavigationCount = 6;

// Default number of days without activity after which a session is considered
// stale and becomes a candidate for garbage collection.
static const size_t kDefaultStaleSessionThresholdDays = 14;  // 2 weeks.

#if defined(OS_ANDROID)
bool IsTabletUI() {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kTabletUI);
}
#endif

sync_pb::SessionHeader::DeviceType GetLocalDeviceType() {
  // TODO(yfriedman): Refactor/combine with "DeviceInformation" code in
  // sync_manager.cc[1060]
#if defined(OS_CHROMEOS)
  return sync_pb::SessionHeader_DeviceType_TYPE_CROS;
#elif defined(OS_LINUX)
  return sync_pb::SessionHeader_DeviceType_TYPE_LINUX;
#elif defined(OS_MACOSX)
  return sync_pb::SessionHeader_DeviceType_TYPE_MAC;
#elif defined(OS_WIN)
  return sync_pb::SessionHeader_DeviceType_TYPE_WIN;
#elif defined(OS_ANDROID)
  return IsTabletUI() ?
      sync_pb::SessionHeader_DeviceType_TYPE_TABLET :
      sync_pb::SessionHeader_DeviceType_TYPE_PHONE;
#else
  return sync_pb::SessionHeader_DeviceType_TYPE_OTHER;
#endif
}

}  // namespace

SessionModelAssociator::SessionModelAssociator(ProfileSyncService* sync_service,
    DataTypeErrorHandler* error_handler)
    : tab_pool_(sync_service),
      local_session_syncid_(syncer::kInvalidId),
      sync_service_(sync_service),
      stale_session_threshold_days_(kDefaultStaleSessionThresholdDays),
      setup_for_test_(false),
      waiting_for_change_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(test_weak_factory_(this)),
      profile_(sync_service->profile()),
      pref_service_(profile_->GetPrefs()),
      error_handler_(error_handler) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_service_);
  DCHECK(profile_);
  if (pref_service_->FindPreference(kSyncSessionsGUID) == NULL) {
    pref_service_->RegisterStringPref(kSyncSessionsGUID,
                                      std::string(),
                                      PrefService::UNSYNCABLE_PREF);
  }
}

SessionModelAssociator::SessionModelAssociator(ProfileSyncService* sync_service,
                                               bool setup_for_test)
    : tab_pool_(sync_service),
      local_session_syncid_(syncer::kInvalidId),
      sync_service_(sync_service),
      stale_session_threshold_days_(kDefaultStaleSessionThresholdDays),
      setup_for_test_(setup_for_test),
      waiting_for_change_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(test_weak_factory_(this)),
      profile_(sync_service->profile()),
      pref_service_(NULL),
      error_handler_(NULL) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_service_);
  DCHECK(profile_);
  DCHECK(setup_for_test);
}

SessionModelAssociator::~SessionModelAssociator() {
  DCHECK(CalledOnValidThread());
}

bool SessionModelAssociator::InitSyncNodeFromChromeId(
    const std::string& id,
    syncer::BaseNode* sync_node) {
  NOTREACHED();
  return false;
}

bool SessionModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(CalledOnValidThread());
  CHECK(has_nodes);
  *has_nodes = false;
  syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  syncer::ReadNode root(&trans);
  if (root.InitByTagLookup(kSessionsTag) != syncer::BaseNode::INIT_OK) {
    LOG(ERROR) << kNoSessionsFolderError;
    return false;
  }
  // The sync model has user created nodes iff the sessions folder has
  // any children.
  *has_nodes = root.HasChildren();
  return true;
}

int64 SessionModelAssociator::GetSyncIdFromChromeId(const size_t& id) {
  DCHECK(CalledOnValidThread());
  return GetSyncIdFromSessionTag(TabIdToTag(GetCurrentMachineTag(), id));
}

int64 SessionModelAssociator::GetSyncIdFromSessionTag(const std::string& tag) {
  DCHECK(CalledOnValidThread());
  syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  syncer::ReadNode node(&trans);
  if (node.InitByClientTagLookup(SESSIONS, tag) != syncer::BaseNode::INIT_OK)
    return syncer::kInvalidId;
  return node.GetId();
}

const SyncedTabDelegate*
SessionModelAssociator::GetChromeNodeFromSyncId(int64 sync_id) {
  NOTREACHED();
  return NULL;
}

bool SessionModelAssociator::InitSyncNodeFromChromeId(
    const size_t& id,
    syncer::BaseNode* sync_node) {
  NOTREACHED();
  return false;
}

bool SessionModelAssociator::AssociateWindows(bool reload_tabs,
                                              syncer::SyncError* error) {
  DCHECK(CalledOnValidThread());
  std::string local_tag = GetCurrentMachineTag();
  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(local_tag);
  sync_pb::SessionHeader* header_s = specifics.mutable_header();
  SyncedSession* current_session =
      synced_session_tracker_.GetSession(local_tag);
  current_session->modified_time = base::Time::Now();
  header_s->set_client_name(current_session_name_);
  header_s->set_device_type(GetLocalDeviceType());

  synced_session_tracker_.ResetSessionTracking(local_tag);
  std::set<SyncedWindowDelegate*> windows =
      SyncedWindowDelegate::GetSyncedWindowDelegates();
  for (std::set<SyncedWindowDelegate*>::const_iterator i =
           windows.begin(); i != windows.end(); ++i) {
    // Make sure the window has tabs and a viewable window. The viewable window
    // check is necessary because, for example, when a browser is closed the
    // destructor is not necessarily run immediately. This means its possible
    // for us to get a handle to a browser that is about to be removed. If
    // the tab count is 0 or the window is NULL, the browser is about to be
    // deleted, so we ignore it.
    if (ShouldSyncWindow(*i) && (*i)->GetTabCount() && (*i)->HasWindow()) {
      sync_pb::SessionWindow window_s;
      SessionID::id_type window_id = (*i)->GetSessionId();
      DVLOG(1) << "Associating window " << window_id << " with "
               << (*i)->GetTabCount() << " tabs.";
      window_s.set_window_id(window_id);
      // Note: We don't bother to set selected tab index anymore. We still
      // consume it when receiving foreign sessions, as reading it is free, but
      // it triggers too many sync cycles with too little value to make setting
      // it worthwhile.
      if ((*i)->IsTypeTabbed()) {
        window_s.set_browser_type(
            sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
      } else {
        window_s.set_browser_type(
            sync_pb::SessionWindow_BrowserType_TYPE_POPUP);
      }

      // Store the order of tabs.
      bool found_tabs = false;
      for (int j = 0; j < (*i)->GetTabCount(); ++j) {
        SessionID::id_type tab_id = (*i)->GetTabIdAt(j);

        if (reload_tabs) {
          SyncedTabDelegate* tab = (*i)->GetTabAt(j);
          // It's possible for GetTabAt to return a null tab if it's not in
          // memory. We can assume this means the tab already existed but hasn't
          // changed, so no need to reassociate.
          if (tab && !AssociateTab(*tab, error)) {
            // Association failed. Either we need to re-associate, or this is an
            // unrecoverable error.
            return false;
          }
        }

        // If the tab is valid, it would have been added to the tracker either
        // by the above AssociateTab call (at association time), or by the
        // change processor calling AssociateTab for all modified tabs.
        // Therefore, we can key whether this window has valid tabs based on
        // the tab's presence in the tracker.
        const SyncedSessionTab* tab;
        if (synced_session_tracker_.LookupSessionTab(local_tag, tab_id, &tab)) {
          found_tabs = true;
          window_s.add_tab(tab_id);
        }
      }
      // Only add a window if it contains valid tabs.
      if (found_tabs) {
        sync_pb::SessionWindow* header_window = header_s->add_window();
        *header_window = window_s;

        // Update this window's representation in the synced session tracker.
        synced_session_tracker_.PutWindowInSession(local_tag, window_id);
        PopulateSessionWindowFromSpecifics(
            local_tag,
            window_s,
            base::Time::Now(),
            current_session->windows[window_id],
            &synced_session_tracker_);
      }
    }
  }
  // Free memory for closed windows and tabs.
  synced_session_tracker_.CleanupSession(local_tag);

  syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  syncer::WriteNode header_node(&trans);
  if (header_node.InitByIdLookup(local_session_syncid_) !=
          syncer::BaseNode::INIT_OK) {
    if (error) {
      *error = error_handler_->CreateAndUploadError(
           FROM_HERE,
           "Failed to load local session header node.",
           model_type());
    }
    return false;
  }
  header_node.SetSessionSpecifics(specifics);
  if (waiting_for_change_) QuitLoopForSubtleTesting();
  return true;
}

// Static.
bool SessionModelAssociator::ShouldSyncWindow(
    const SyncedWindowDelegate* window) {
  if (window->IsApp())
    return false;
  return window->IsTypeTabbed() || window->IsTypePopup();
}

bool SessionModelAssociator::AssociateTabs(
    const std::vector<SyncedTabDelegate*>& tabs,
    syncer::SyncError* error) {
  DCHECK(CalledOnValidThread());
  for (std::vector<SyncedTabDelegate*>::const_iterator i = tabs.begin();
       i != tabs.end();
       ++i) {
    if (!AssociateTab(**i, error))
      return false;
  }
  if (waiting_for_change_) QuitLoopForSubtleTesting();
  return true;
}

bool SessionModelAssociator::AssociateTab(const SyncedTabDelegate& tab,
                                          syncer::SyncError* error) {
  DCHECK(CalledOnValidThread());
  int64 sync_id;
  SessionID::id_type tab_id = tab.GetSessionId();
  if (tab.IsBeingDestroyed()) {
    // This tab is closing.
    TabLinksMap::iterator tab_iter = tab_map_.find(tab_id);
    if (tab_iter == tab_map_.end()) {
      // We aren't tracking this tab (for example, sync setting page).
      return true;
    }
    tab_pool_.FreeTabNode(tab_iter->second->sync_id());
    load_consumer_.CancelAllRequestsForClientData(tab_id);
    tab_map_.erase(tab_iter);
    return true;
  }

  if (!ShouldSyncTab(tab))
    return true;

  TabLinksMap::iterator tab_map_iter = tab_map_.find(tab_id);
  TabLink* tab_link = NULL;
  if (tab_map_iter == tab_map_.end()) {
    // This is a new tab, get a sync node for it.
    sync_id = tab_pool_.GetFreeTabNode();
    if (sync_id == syncer::kInvalidId) {
      if (error) {
        *error = error_handler_->CreateAndUploadError(
            FROM_HERE,
            "Received invalid tab node from tab pool.",
            model_type());
      }
      return false;
    }
    tab_link = new TabLink(sync_id, &tab);
    tab_map_[tab_id] = make_linked_ptr<TabLink>(tab_link);
  } else {
    // This tab is already associated with a sync node, reuse it.
    // Note: on some platforms the tab object may have changed, so we ensure
    // the tab link is up to date.
    tab_link = tab_map_iter->second.get();
    tab_map_iter->second->set_tab(&tab);
  }
  DCHECK(tab_link);
  DCHECK_NE(tab_link->sync_id(), syncer::kInvalidId);

  DVLOG(1) << "Reloading tab " << tab_id << " from window "
           << tab.GetWindowId();
  return WriteTabContentsToSyncModel(tab_link, error);
}

bool SessionModelAssociator::WriteTabContentsToSyncModel(
    TabLink* tab_link,
    syncer::SyncError* error) {
  DCHECK(CalledOnValidThread());
  const SyncedTabDelegate& tab = *(tab_link->tab());
  const SyncedWindowDelegate& window =
      *SyncedWindowDelegate::FindSyncedWindowDelegateWithId(
          tab.GetWindowId());
  int64 sync_id = tab_link->sync_id();
  GURL old_tab_url = tab_link->url();

  // Load the last stored version of this tab so we can compare changes. If this
  // is a new tab, session_tab will be a blank/newly created SessionTab object.
  SyncedSessionTab* session_tab =
      synced_session_tracker_.GetTab(GetCurrentMachineTag(),
                                     tab.GetSessionId());

  // We build a clean session tab specifics directly from the tab data.
  sync_pb::SessionTab tab_s;

  GURL new_url;
  AssociateTabContents(window, tab, session_tab, &tab_s, &new_url);

  // Trigger the favicon load if needed. We do this before opening the write
  // transaction to avoid jank.
  tab_link->set_url(new_url);
  if (new_url != old_tab_url) {
    LoadFaviconForTab(tab_link);
  }

  // Update our last modified time.
  synced_session_tracker_.GetSession(GetCurrentMachineTag())->modified_time =
      base::Time::Now();

  syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  syncer::WriteNode tab_node(&trans);
  if (tab_node.InitByIdLookup(sync_id) != syncer::BaseNode::INIT_OK) {
    if (error) {
      *error = error_handler_->CreateAndUploadError(
          FROM_HERE,
          "Failed to look up local tab node",
          model_type());
    }
    return false;
  }

  sync_pb::SessionSpecifics specifics = tab_node.GetSessionSpecifics();
  if (new_url == old_tab_url) {
    // Load the old specifics and copy over the favicon data if needed.
    // TODO(zea): store local favicons in the |synced_favicons_| map and use
    // that instead of reading from sync. This will be necessary to switch to
    // the new api.
    tab_s.set_favicon(specifics.tab().favicon());
    tab_s.set_favicon_source(specifics.tab().favicon_source());
    tab_s.set_favicon_type(specifics.tab().favicon_type());
  }
  // Retain the base SessionSpecifics data (tag, tab_node_id, etc.), and just
  // write the new SessionTabSpecifics.
  specifics.mutable_tab()->CopyFrom(tab_s);

  // Write into the actual sync model.
  tab_node.SetSessionSpecifics(specifics);

  return true;
}

// Builds |sync_tab| by combining data from |prev_tab| and |new_tab|. Updates
// |prev_tab| to reflect the newest version.
// Timestamps are chosen from either |prev_tab| or base::Time::Now() based on
// the following rules:
// 1. If a navigation exists in both |new_tab| and |prev_tab|, as determined
//    by the unique id, and the navigation didn't just become the current
//    navigation, we preserve the old timestamp.
// 2. If the navigation exists in both but just become the current navigation
//    (e.g. the user went back in history to this navigation), we update the
//    timestamp to Now().
// 3. All new navigations not present in |prev_tab| have their timestamps set to
//    Now().
void SessionModelAssociator::AssociateTabContents(
    const SyncedWindowDelegate& window,
    const SyncedTabDelegate& new_tab,
    SyncedSessionTab* prev_tab,
    sync_pb::SessionTab* sync_tab,
    GURL* new_url) {
  DCHECK(prev_tab);
  DCHECK(sync_tab);
  DCHECK(new_url);
  SessionID::id_type tab_id = new_tab.GetSessionId();
  sync_tab->set_tab_id(tab_id);
  sync_tab->set_window_id(new_tab.GetWindowId());
  const int current_index = new_tab.GetCurrentEntryIndex();
  sync_tab->set_current_navigation_index(current_index);
  const int min_index = std::max(0,
                                 current_index - kMaxSyncNavigationCount);
  const int max_index = std::min(current_index + kMaxSyncNavigationCount,
                                 new_tab.GetEntryCount());
  const int pending_index = new_tab.GetPendingEntryIndex();
  sync_tab->set_pinned(window.IsTabPinned(&new_tab));
  if (new_tab.HasExtensionAppId()) {
    sync_tab->set_extension_app_id(new_tab.GetExtensionAppId());
  }

  sync_tab->mutable_navigation()->Clear();
  std::vector<SyncedTabNavigation>::const_iterator prev_nav_iter =
      prev_tab->synced_tab_navigations.begin();
  for (int i = min_index; i < max_index; ++i) {
    const NavigationEntry* entry = (i == pending_index) ?
       new_tab.GetPendingEntry() : new_tab.GetEntryAtIndex(i);
    DCHECK(entry);
    if (i == min_index) {
      // Find the location of the first navigation within the previous list of
      // navigations. We only need to do this once, as all subsequent
      // navigations are either contiguous or completely new.
      for (;prev_nav_iter != prev_tab->synced_tab_navigations.end();
           ++prev_nav_iter) {
        if (prev_nav_iter->unique_id() == entry->GetUniqueID())
          break;
      }
    }
    if (entry->GetVirtualURL().is_valid()) {
      if (i == current_index) {
        *new_url = GURL(entry->GetVirtualURL().spec());
        DVLOG(1) << "Associating local tab " << new_tab.GetSessionId()
                 << " with url " << new_url->spec() << " and title "
                 << entry->GetTitle();

      }
      sync_pb::TabNavigation* sync_nav = sync_tab->add_navigation();
      PopulateSessionSpecificsNavigation(*entry, sync_nav);

      // If this navigation is an old one, reuse the old timestamp. Otherwise we
      // leave the timestamp as the current time.
      if (prev_nav_iter != prev_tab->synced_tab_navigations.end() &&
          prev_nav_iter->unique_id() == entry->GetUniqueID()) {
        // Check that we haven't gone back/foward in the nav stack to this page
        // (if so, we want to refresh the timestamp).
        if (!(current_index != prev_tab->current_navigation_index &&
              current_index == i)) {
          sync_nav->set_timestamp(
              syncer::TimeToProtoTime(prev_nav_iter->timestamp()));
          DVLOG(2) << "Nav to " << sync_nav->virtual_url() << " already known, "
                   << "reusing old timestamp " << sync_nav->timestamp();
        }
        // Even if the user went back in their history, they may have skipped
        // over navigations, so the subsequent navigation entries may need their
        // old timestamps preserved.
        ++prev_nav_iter;
      } else if (current_index != i &&
                 prev_tab->synced_tab_navigations.empty()) {
        // If this is a new tab, and has more than one navigation, we don't
        // actually want to assign the current timestamp to other navigations.
        // Override the timestamp to 0 in that case.
        // Note: this is primarily to handle restoring sessions at restart,
        // opening recently closed tabs, or opening tabs from other devices.
        // Only the current navigation should have a timestamp in those cases.
        sync_nav->set_timestamp(0);
      }
    }
  }

  // Now update our local version with the newest data.
  PopulateSessionTabFromSpecifics(*sync_tab,
                                  base::Time::Now(),
                                  prev_tab);
}

void SessionModelAssociator::LoadFaviconForTab(TabLink* tab_link) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kSyncTabFavicons))
    return;
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;
  SessionID::id_type tab_id = tab_link->tab()->GetSessionId();
  if (tab_link->favicon_load_handle()) {
    // We have an outstanding favicon load for this tab. Cancel it.
    load_consumer_.CancelAllRequestsForClientData(tab_id);
  }
  DVLOG(1) << "Triggering favicon load for url " << tab_link->url().spec();
  FaviconService::Handle handle = favicon_service->GetRawFaviconForURL(
      FaviconService::FaviconForURLParams(profile_, tab_link->url(),
          history::FAVICON, gfx::kFaviconSize, &load_consumer_),
      ui::SCALE_FACTOR_100P,
      base::Bind(&SessionModelAssociator::OnFaviconDataAvailable,
                 AsWeakPtr()));
  load_consumer_.SetClientData(favicon_service, handle, tab_id);
  tab_link->set_favicon_load_handle(handle);
}

void SessionModelAssociator::OnFaviconDataAvailable(
    FaviconService::Handle handle,
    const history::FaviconBitmapResult& bitmap_result) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kSyncTabFavicons))
    return;
  SessionID::id_type tab_id =
      load_consumer_.GetClientData(
          FaviconServiceFactory::GetForProfile(
              profile_, Profile::EXPLICIT_ACCESS), handle);
  TabLinksMap::iterator iter = tab_map_.find(tab_id);
  if (iter == tab_map_.end()) {
    DVLOG(1) << "Ignoring favicon for closed tab " << tab_id;
    return;
  }
  TabLink* tab_link = iter->second.get();
  DCHECK(tab_link);
  DCHECK(tab_link->url().is_valid());
  // The tab_link holds the current url. Because this load request would have
  // been canceled if the url had changed, we know the url must still be
  // up to date.

  if (bitmap_result.is_valid()) {
    DCHECK_EQ(handle, tab_link->favicon_load_handle());
    tab_link->set_favicon_load_handle(0);
    DCHECK_EQ(bitmap_result.icon_type, history::FAVICON);
    DCHECK_NE(tab_link->sync_id(), syncer::kInvalidId);
    // Load the sync tab node and update the favicon data.
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::WriteNode tab_node(&trans);
    if (tab_node.InitByIdLookup(tab_link->sync_id()) !=
            syncer::BaseNode::INIT_OK) {
      LOG(WARNING) << "Failed to load sync tab node for tab id " << tab_id
                   << " and url " << tab_link->url().spec();
      return;
    }
    sync_pb::SessionSpecifics session_specifics =
        tab_node.GetSessionSpecifics();
    DCHECK(session_specifics.has_tab());
    sync_pb::SessionTab* tab = session_specifics.mutable_tab();
    if (bitmap_result.bitmap_data->size() > 0) {
      DVLOG(1) << "Storing session favicon for "
               << tab_link->url() << " with size "
               << bitmap_result.bitmap_data->size() << " bytes.";
      tab->set_favicon(bitmap_result.bitmap_data->front(),
                       bitmap_result.bitmap_data->size());
      tab->set_favicon_type(sync_pb::SessionTab::TYPE_WEB_FAVICON);
      tab->set_favicon_source(bitmap_result.icon_url.spec());
    } else {
      LOG(WARNING) << "Null favicon stored for url " << tab_link->url().spec();
    }
    tab_node.SetSessionSpecifics(session_specifics);
  } else {
    // Else the favicon either isn't loaded yet or there is no favicon. We
    // deliberately don't clear the tab_link's favicon_load_handle so we know
    // that we're still waiting for a favicon. ReceivedFavicons(..) below will
    // trigger another favicon load once/if the favicon for the current url
    // becomes available.
    DVLOG(1) << "Favicon load failed for url " << tab_link->url().spec();
  }
}

void SessionModelAssociator::FaviconsUpdated(
    const std::set<GURL>& urls) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kSyncTabFavicons))
    return;

  // TODO(zea): consider a separate container for tabs with outstanding favicon
  // loads so we don't have to iterate through all tabs comparing urls.
  for (std::set<GURL>::const_iterator i = urls.begin(); i != urls.end(); ++i) {
    for (TabLinksMap::iterator tab_iter = tab_map_.begin();
         tab_iter != tab_map_.end(); ++tab_iter) {
      // Only update the tab's favicon if it doesn't already have one (i.e.
      // favicon_load_handle is not 0). Otherwise we can get into a situation
      // where we rewrite tab specifics every time a favicon changes, since some
      // favicons can in fact be web-controlled/animated.
      if (tab_iter->second->url() == *i &&
          tab_iter->second->favicon_load_handle() != 0) {
        LoadFaviconForTab(tab_iter->second.get());
      }
    }
  }
}

// Static
// TODO(zea): perhaps sync state (scroll position, form entries, etc.) as well?
// See http://crbug.com/67068.
void SessionModelAssociator::PopulateSessionSpecificsNavigation(
    const NavigationEntry& navigation,
    sync_pb::TabNavigation* tab_navigation) {
  tab_navigation->set_virtual_url(navigation.GetVirtualURL().spec());
  // FIXME(zea): Support referrer policy?
  tab_navigation->set_referrer(navigation.GetReferrer().url.spec());
  tab_navigation->set_title(UTF16ToUTF8(navigation.GetTitle()));
  switch (navigation.GetTransitionType()) {
    case content::PAGE_TRANSITION_LINK:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_LINK);
      break;
    case content::PAGE_TRANSITION_TYPED:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_TYPED);
      break;
    case content::PAGE_TRANSITION_AUTO_BOOKMARK:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK);
      break;
    case content::PAGE_TRANSITION_AUTO_SUBFRAME:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME);
      break;
    case content::PAGE_TRANSITION_MANUAL_SUBFRAME:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_MANUAL_SUBFRAME);
      break;
    case content::PAGE_TRANSITION_GENERATED:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_GENERATED);
      break;
    case content::PAGE_TRANSITION_AUTO_TOPLEVEL:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_AUTO_TOPLEVEL);
      break;
    case content::PAGE_TRANSITION_FORM_SUBMIT:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_FORM_SUBMIT);
      break;
    case content::PAGE_TRANSITION_RELOAD:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_RELOAD);
      break;
    case content::PAGE_TRANSITION_KEYWORD:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_KEYWORD);
      break;
    case content::PAGE_TRANSITION_KEYWORD_GENERATED:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_KEYWORD_GENERATED);
      break;
    case content::PAGE_TRANSITION_CHAIN_START:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_CHAIN_START);
      break;
    case content::PAGE_TRANSITION_CHAIN_END:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_CHAIN_END);
      break;
    case content::PAGE_TRANSITION_CLIENT_REDIRECT:
      tab_navigation->set_navigation_qualifier(
        sync_pb::SyncEnums_PageTransitionQualifier_CLIENT_REDIRECT);
      break;
    case content::PAGE_TRANSITION_SERVER_REDIRECT:
      tab_navigation->set_navigation_qualifier(
        sync_pb::SyncEnums_PageTransitionQualifier_SERVER_REDIRECT);
      break;
    default:
      tab_navigation->set_page_transition(
        sync_pb::SyncEnums_PageTransition_TYPED);
  }
  tab_navigation->set_unique_id(navigation.GetUniqueID());
  tab_navigation->set_timestamp(
      syncer::TimeToProtoTime(base::Time::Now()));
}

void SessionModelAssociator::Associate(const SyncedTabDelegate* tab,
                                       int64 sync_id) {
  NOTIMPLEMENTED();
}

void SessionModelAssociator::Disassociate(int64 sync_id) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

syncer::SyncError SessionModelAssociator::AssociateModels() {
  DCHECK(CalledOnValidThread());
  syncer::SyncError error;

  // Ensure that we disassociated properly, otherwise memory might leak.
  DCHECK(synced_session_tracker_.Empty());
  DCHECK_EQ(0U, tab_pool_.capacity());

  local_session_syncid_ = syncer::kInvalidId;

  // Read any available foreign sessions and load any session data we may have.
  // If we don't have any local session data in the db, create a header node.
  {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());

    syncer::ReadNode root(&trans);
    if (root.InitByTagLookup(syncer::ModelTypeToRootTag(model_type())) !=
            syncer::BaseNode::INIT_OK) {
      return error_handler_->CreateAndUploadError(
          FROM_HERE,
          kNoSessionsFolderError,
          model_type());
    }

    // Make sure we have a machine tag.
    if (current_machine_tag_.empty()) {
      InitializeCurrentMachineTag(&trans);
      // The session name is retrieved asynchronously so it might not come back
      // for the writing of the session. However, we write to the session often
      // enough (on every navigation) that we'll pick it up quickly.
      InitializeCurrentSessionName();
    }
    synced_session_tracker_.SetLocalSessionTag(current_machine_tag_);
    if (!UpdateAssociationsFromSyncModel(root, &trans, &error)) {
      DCHECK(error.IsSet());
      return error;
    }

    if (local_session_syncid_ == syncer::kInvalidId) {
      // The sync db didn't have a header node for us, we need to create one.
      syncer::WriteNode write_node(&trans);
      syncer::WriteNode::InitUniqueByCreationResult result =
          write_node.InitUniqueByCreation(SESSIONS, root, current_machine_tag_);
      if (result != syncer::WriteNode::INIT_SUCCESS) {
        // If we can't look it up, and we can't create it, chances are there's
        // a pre-existing node that has encryption issues. But, since we can't
        // load the item, we can't remove it, and error out at this point.
        return error_handler_->CreateAndUploadError(
            FROM_HERE,
            "Failed to create sessions header sync node.",
            model_type());
      }

      // Write the initial values to the specifics so that in case of a crash or
      // error we don't persist a half-written node.
      write_node.SetTitle(UTF8ToWide(current_machine_tag_));
      sync_pb::SessionSpecifics base_specifics;
      base_specifics.set_session_tag(current_machine_tag_);
      sync_pb::SessionHeader* header_s = base_specifics.mutable_header();
      header_s->set_client_name(current_session_name_);
      header_s->set_device_type(GetLocalDeviceType());
      write_node.SetSessionSpecifics(base_specifics);

      local_session_syncid_ = write_node.GetId();
    }
  }

  // Check if anything has changed on the client side.
  if (!UpdateSyncModelDataFromClient(&error)) {
    DCHECK(error.IsSet());
    return error;
  }

  DVLOG(1) << "Session models associated.";
  DCHECK(!error.IsSet());
  return error;
}

syncer::SyncError SessionModelAssociator::DisassociateModels() {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Disassociating local session " << GetCurrentMachineTag();
  synced_session_tracker_.Clear();
  tab_map_.clear();
  tab_pool_.clear();
  local_session_syncid_ = syncer::kInvalidId;
  current_machine_tag_ = "";
  current_session_name_ = "";
  load_consumer_.CancelAllRequests();
  synced_favicons_.clear();
  synced_favicon_pages_.clear();

  // There is no local model stored with which to disassociate, just notify
  // foreign session handlers.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FOREIGN_SESSION_DISABLED,
      content::Source<Profile>(sync_service_->profile()),
      content::NotificationService::NoDetails());
  return syncer::SyncError();
}

void SessionModelAssociator::InitializeCurrentMachineTag(
    syncer::WriteTransaction* trans) {
  DCHECK(CalledOnValidThread());
  DCHECK(current_machine_tag_.empty());
  std::string persisted_guid;
  if (pref_service_)
    persisted_guid = pref_service_->GetString(kSyncSessionsGUID);
  if (!persisted_guid.empty()) {
    current_machine_tag_ = persisted_guid;
    DVLOG(1) << "Restoring persisted session sync guid: "
             << persisted_guid;
  } else {
    syncer::syncable::Directory* dir =
        trans->GetWrappedWriteTrans()->directory();
    current_machine_tag_ = "session_sync";
#if defined(OS_ANDROID)
    const std::string android_id = syncer::internal::GetAndroidId();
    // There are reports that sometimes the android_id can't be read. Those
    // are supposed to be fixed as of Gingerbread, but if it happens we fall
    // back to use the same GUID generation as on other platforms.
    current_machine_tag_.append(android_id.empty() ?
                                    dir->cache_guid() : android_id);
#else
    current_machine_tag_.append(dir->cache_guid());
#endif
    DVLOG(1) << "Creating session sync guid: " << current_machine_tag_;
    if (pref_service_)
      pref_service_->SetString(kSyncSessionsGUID, current_machine_tag_);
  }

  tab_pool_.set_machine_tag(current_machine_tag_);
}

void SessionModelAssociator::OnSessionNameInitialized(
    const std::string& name) {
  DCHECK(CalledOnValidThread());
  // Only use the default machine name if it hasn't already been set.
  if (current_session_name_.empty()) {
    current_session_name_ = name;
    // Force a reassociation so we update our header node with the current name.
    // TODO(zea): Pull the name from somewhere shared with the sync manager.
    // crbug.com/124287
    SessionModelAssociator::AssociateWindows(false, NULL);
  }
}

bool SessionModelAssociator::GetSyncedFaviconForPageURL(
    const std::string& url,
    std::string* png_favicon) const {
  std::map<std::string, std::string>::const_iterator iter =
      synced_favicon_pages_.find(url);
  if (iter == synced_favicon_pages_.end())
    return false;
  DCHECK(synced_favicons_.find(iter->second) != synced_favicons_.end());
  const std::string& favicon =
      synced_favicons_.find(iter->second)->second->data;
  png_favicon->assign(favicon);
  DCHECK_GT(favicon.size(), 0U);
  return true;
}

void SessionModelAssociator::InitializeCurrentSessionName() {
  DCHECK(CalledOnValidThread());
  if (setup_for_test_) {
    // We post this task to break out of any transactional locks a caller may be
    // holding.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&SessionModelAssociator::OnSessionNameInitialized,
                   AsWeakPtr(),
                   std::string("TestSessionName")));
  } else {
    syncer::GetSessionName(
        BrowserThread::GetBlockingPool(),
        base::Bind(&SessionModelAssociator::OnSessionNameInitialized,
                   AsWeakPtr()));
  }
}

bool SessionModelAssociator::UpdateAssociationsFromSyncModel(
    const syncer::ReadNode& root,
    syncer::WriteTransaction* trans,
    syncer::SyncError* error) {
  DCHECK(CalledOnValidThread());
  DCHECK(tab_pool_.empty());
  DCHECK_EQ(local_session_syncid_, syncer::kInvalidId);

  // Iterate through the nodes and associate any foreign sessions.
  int64 id = root.GetFirstChildId();
  while (id != syncer::kInvalidId) {
    syncer::WriteNode sync_node(trans);
    if (sync_node.InitByIdLookup(id) != syncer::BaseNode::INIT_OK) {
      if (error) {
        *error = error_handler_->CreateAndUploadError(
            FROM_HERE,
            "Failed to load sync node",
            model_type());
      }
      return false;
    }
    int64 next_id = sync_node.GetSuccessorId();

    const sync_pb::SessionSpecifics& specifics =
        sync_node.GetSessionSpecifics();
    const base::Time& modification_time = sync_node.GetModificationTime();
    if (specifics.session_tag().empty()) {
      // This is a corrupted node. Just delete it.
      LOG(WARNING) << "Found node with no session tag, deleting.";
      sync_node.Remove();
    } else if (specifics.session_tag() != GetCurrentMachineTag()) {
      AssociateForeignSpecifics(specifics, modification_time);
    } else {
      // This is previously stored local session information.
      if (specifics.has_header() &&
          local_session_syncid_ == syncer::kInvalidId) {
        // This is our previous header node, reuse it.
        local_session_syncid_ = id;
        if (specifics.header().has_client_name()) {
          current_session_name_ = specifics.header().client_name();
        }
      } else {
        if (specifics.has_header()) {
          LOG(WARNING) << "Found more than one session header node with local "
                       << " tag.";
        } else if (!specifics.has_tab()) {
          LOG(WARNING) << "Found local node with no header or tag field.";
        }

        // TODO(zea): fix this once we add support for reassociating
        // pre-existing tabs with pre-existing tab nodes. We'll need to load
        // the tab_node_id and ensure the tab_pool_ keeps track of them.
        sync_node.Remove();
      }
    }
    id = next_id;
  }

  // After updating from sync model all tabid's should be free.
  DCHECK(tab_pool_.full());
  return true;
}

void SessionModelAssociator::AssociateForeignSpecifics(
    const sync_pb::SessionSpecifics& specifics,
    const base::Time& modification_time) {
  DCHECK(CalledOnValidThread());
  std::string foreign_session_tag = specifics.session_tag();
  if (foreign_session_tag == GetCurrentMachineTag() && !setup_for_test_)
    return;

  SyncedSession* foreign_session =
      synced_session_tracker_.GetSession(foreign_session_tag);
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
    synced_session_tracker_.ResetSessionTracking(foreign_session_tag);

    // Process all the windows and their tab information.
    int num_windows = header.window_size();
    DVLOG(1) << "Associating " << foreign_session_tag << " with "
             << num_windows << " windows.";
    for (int i = 0; i < num_windows; ++i) {
      const sync_pb::SessionWindow& window_s = header.window(i);
      SessionID::id_type window_id = window_s.window_id();
      synced_session_tracker_.PutWindowInSession(foreign_session_tag,
                                                 window_id);
      PopulateSessionWindowFromSpecifics(foreign_session_tag,
                                         window_s,
                                         modification_time,
                                         foreign_session->windows[window_id],
                                         &synced_session_tracker_);
    }

    // Delete any closed windows and unused tabs as necessary.
    synced_session_tracker_.CleanupSession(foreign_session_tag);
  } else if (specifics.has_tab()) {
    const sync_pb::SessionTab& tab_s = specifics.tab();
    SessionID::id_type tab_id = tab_s.tab_id();
    SyncedSessionTab* tab =
        synced_session_tracker_.GetTab(foreign_session_tag, tab_id);

    // Figure out what the previous url for this tab was (may be empty string
    // if this is a new tab).
    std::string previous_url;
    if (tab->navigations.size() > 0) {
      int selected_index = tab->current_navigation_index;
      selected_index = std::max(
          0,
          std::min(selected_index,
                   static_cast<int>(tab->navigations.size() - 1)));
      if (tab->navigations[selected_index].virtual_url().is_valid())
        previous_url = tab->navigations[selected_index].virtual_url().spec();
      if (synced_favicon_pages_.find(previous_url) ==
          synced_favicon_pages_.end()) {
        // The previous url didn't have a favicon. No need to decrement it.
        previous_url.clear();
      }
    }

    // Update SessionTab based on protobuf.
    PopulateSessionTabFromSpecifics(tab_s, modification_time, tab);

    // Loads the tab favicon, increments the usage counter, and updates
    // synced_favicon_pages_.
    LoadForeignTabFavicon(tab_s);

    // Now check to see if the favicon associated with the previous url is no
    // longer in use. This will have no effect if the current url matches the
    // previous url (LoadForeignTabFavicon increments, this decrements, no net
    // change in usage), or if the previous_url was not set (new tab).
    DecrementAndCleanFaviconForURL(previous_url);

    // Update the last modified time.
    if (foreign_session->modified_time < modification_time)
      foreign_session->modified_time = modification_time;
  } else {
    LOG(WARNING) << "Ignoring foreign session node with missing header/tab "
                 << "fields and tag " << foreign_session_tag << ".";
  }
}

void SessionModelAssociator::DecrementAndCleanFaviconForURL(
    const std::string& page_url) {
  if (page_url.empty())
    return;
  std::map<std::string, std::string>::const_iterator iter =
      synced_favicon_pages_.find(page_url);
  if (iter != synced_favicon_pages_.end()) {
    std::string favicon_url = iter->second;
    DCHECK_GT(synced_favicons_[favicon_url]->usage_count, 0);
    --(synced_favicons_[favicon_url]->usage_count);
    if (synced_favicons_[favicon_url]->usage_count <= 0) {
      // No more tabs using this favicon. Erase it.
      synced_favicons_.erase(favicon_url);
      // Erase the page mappings to the favicon url. We iterate through all
      // page urls in case multiple pages share the same favicon.
      std::map<std::string, std::string>::iterator page_iter;
      for (page_iter = synced_favicon_pages_.begin();
           page_iter != synced_favicon_pages_.end();) {
        std::map<std::string, std::string>::iterator to_delete = page_iter;
        ++page_iter;
        if (to_delete->second == favicon_url) {
          synced_favicon_pages_.erase(to_delete);
        }
      }
    }
  }
}

size_t SessionModelAssociator::NumFaviconsForTesting() const {
  return synced_favicons_.size();
}

bool SessionModelAssociator::DisassociateForeignSession(
    const std::string& foreign_session_tag) {
  DCHECK(CalledOnValidThread());
  if (foreign_session_tag == GetCurrentMachineTag()) {
    DVLOG(1) << "Local session deleted! Doing nothing until a navigation is "
             << "triggered.";
    return false;
  }
  DVLOG(1) << "Disassociating session " << foreign_session_tag;
  return synced_session_tracker_.DeleteSession(foreign_session_tag);
}

// Static
void SessionModelAssociator::PopulateSessionHeaderFromSpecifics(
    const sync_pb::SessionHeader& header_specifics,
    const base::Time& mtime,
    SyncedSession* session_header) {
  if (header_specifics.has_client_name()) {
    session_header->session_name = header_specifics.client_name();
  }
  if (header_specifics.has_device_type()) {
    switch (header_specifics.device_type()) {
      case sync_pb::SessionHeader_DeviceType_TYPE_WIN:
        session_header->device_type = SyncedSession::TYPE_WIN;
        break;
      case sync_pb::SessionHeader_DeviceType_TYPE_MAC:
        session_header->device_type = SyncedSession::TYPE_MACOSX;
        break;
      case sync_pb::SessionHeader_DeviceType_TYPE_LINUX:
        session_header->device_type = SyncedSession::TYPE_LINUX;
        break;
      case sync_pb::SessionHeader_DeviceType_TYPE_CROS:
        session_header->device_type = SyncedSession::TYPE_CHROMEOS;
        break;
      case sync_pb::SessionHeader_DeviceType_TYPE_PHONE:
        session_header->device_type = SyncedSession::TYPE_PHONE;
        break;
      case sync_pb::SessionHeader_DeviceType_TYPE_TABLET:
        session_header->device_type = SyncedSession::TYPE_TABLET;
        break;
      case sync_pb::SessionHeader_DeviceType_TYPE_OTHER:
        // Intentionally fall-through
      default:
        session_header->device_type = SyncedSession::TYPE_OTHER;
        break;
    }
  }
  session_header->modified_time = mtime;
}

// Static
void SessionModelAssociator::PopulateSessionWindowFromSpecifics(
    const std::string& session_tag,
    const sync_pb::SessionWindow& specifics,
    const base::Time& mtime,
    SessionWindow* session_window,
    SyncedSessionTracker* tracker) {
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
    tracker->PutTabInWindow(session_tag,
                            session_window->window_id.id(),
                            tab_id,
                            i);
  }
}

// Static
void SessionModelAssociator::PopulateSessionTabFromSpecifics(
    const sync_pb::SessionTab& specifics,
    const base::Time& mtime,
    SyncedSessionTab* tab) {
  DCHECK_EQ(tab->tab_id.id(), specifics.tab_id());
  if (specifics.has_tab_id())
    tab->tab_id.set_id(specifics.tab_id());
  if (specifics.has_window_id())
    tab->window_id.set_id(specifics.window_id());
  if (specifics.has_tab_visual_index())
    tab->tab_visual_index = specifics.tab_visual_index();
  if (specifics.has_current_navigation_index())
    tab->current_navigation_index = specifics.current_navigation_index();
  if (specifics.has_pinned())
    tab->pinned = specifics.pinned();
  if (specifics.has_extension_app_id())
    tab->extension_app_id = specifics.extension_app_id();
  tab->timestamp = mtime;
  // Cleared in case we reuse a pre-existing SyncedSessionTab object.
  tab->navigations.clear();
  tab->synced_tab_navigations.clear();
  for (int i = 0; i < specifics.navigation_size(); ++i) {
    AppendSessionTabNavigation(specifics.navigation(i),
                               tab);
  }
}

// Static
void SessionModelAssociator::AppendSessionTabNavigation(
    const sync_pb::TabNavigation& specifics,
    SyncedSessionTab* tab) {
  int index = 0;
  GURL virtual_url;
  GURL referrer;
  string16 title;
  std::string state;
  content::PageTransition transition(content::PAGE_TRANSITION_LINK);
  base::Time timestamp;
  int unique_id = 0;
  if (specifics.has_virtual_url()) {
    GURL gurl(specifics.virtual_url());
    virtual_url = gurl;
  }
  if (specifics.has_referrer()) {
    GURL gurl(specifics.referrer());
    referrer = gurl;
  }
  if (specifics.has_title())
    title = UTF8ToUTF16(specifics.title());
  if (specifics.has_state())
    state = specifics.state();
  if (specifics.has_page_transition() ||
      specifics.has_navigation_qualifier()) {
    switch (specifics.page_transition()) {
      case sync_pb::SyncEnums_PageTransition_LINK:
        transition = content::PAGE_TRANSITION_LINK;
        break;
      case sync_pb::SyncEnums_PageTransition_TYPED:
        transition = content::PAGE_TRANSITION_TYPED;
        break;
      case sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK:
        transition = content::PAGE_TRANSITION_AUTO_BOOKMARK;
        break;
      case sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME:
        transition = content::PAGE_TRANSITION_AUTO_SUBFRAME;
        break;
      case sync_pb::SyncEnums_PageTransition_MANUAL_SUBFRAME:
        transition = content::PAGE_TRANSITION_MANUAL_SUBFRAME;
        break;
      case sync_pb::SyncEnums_PageTransition_GENERATED:
        transition = content::PAGE_TRANSITION_GENERATED;
        break;
      case sync_pb::SyncEnums_PageTransition_AUTO_TOPLEVEL:
        transition = content::PAGE_TRANSITION_AUTO_TOPLEVEL;
        break;
      case sync_pb::SyncEnums_PageTransition_FORM_SUBMIT:
        transition = content::PAGE_TRANSITION_FORM_SUBMIT;
        break;
      case sync_pb::SyncEnums_PageTransition_RELOAD:
        transition = content::PAGE_TRANSITION_RELOAD;
        break;
      case sync_pb::SyncEnums_PageTransition_KEYWORD:
        transition = content::PAGE_TRANSITION_KEYWORD;
        break;
      case sync_pb::SyncEnums_PageTransition_KEYWORD_GENERATED:
        transition = content::PAGE_TRANSITION_KEYWORD_GENERATED;
        break;
      case sync_pb::SyncEnums_PageTransition_CHAIN_START:
        transition = content::PAGE_TRANSITION_CHAIN_START;
        break;
      case sync_pb::SyncEnums_PageTransition_CHAIN_END:
        transition = content::PAGE_TRANSITION_CHAIN_END;
        break;
      default:
        switch (specifics.navigation_qualifier()) {
          case sync_pb::SyncEnums_PageTransitionQualifier_CLIENT_REDIRECT:
            transition = content::PAGE_TRANSITION_CLIENT_REDIRECT;
            break;
            case sync_pb::SyncEnums_PageTransitionQualifier_SERVER_REDIRECT:
            transition = content::PAGE_TRANSITION_SERVER_REDIRECT;
              break;
            default:
            transition = content::PAGE_TRANSITION_TYPED;
        }
    }
  }
  if (specifics.has_timestamp()) {
    timestamp = syncer::ProtoTimeToTime(specifics.timestamp());
  }
  if (specifics.has_unique_id()) {
    unique_id = specifics.unique_id();
  }
  SyncedTabNavigation tab_navigation(
      index, virtual_url,
      content::Referrer(referrer, WebKit::WebReferrerPolicyDefault), title,
      state, transition, unique_id, timestamp);
  // We insert it twice, once for our SyncedTabNavigations, once for the normal
  // TabNavigation (used by the session restore UI).
  tab->synced_tab_navigations.insert(tab->synced_tab_navigations.end(),
                                     tab_navigation);
  tab->navigations.insert(tab->navigations.end(),
                          tab_navigation);
}

void SessionModelAssociator::LoadForeignTabFavicon(
    const sync_pb::SessionTab& tab) {
  if (!tab.has_favicon() || tab.favicon().empty())
    return;
  if (!tab.has_favicon_type() ||
      tab.favicon_type() != sync_pb::SessionTab::TYPE_WEB_FAVICON) {
    DVLOG(1) << "Ignoring non-web favicon.";
    return;
  }
  if (tab.navigation_size() == 0)
    return;
  int selected_index = tab.current_navigation_index();
  selected_index = std::max(
      0,
      std::min(selected_index,
               static_cast<int>(tab.navigation_size() - 1)));
  GURL navigation_url(tab.navigation(selected_index).virtual_url());
  if (!navigation_url.is_valid())
    return;
  GURL favicon_source(tab.favicon_source());
  if (!favicon_source.is_valid())
    return;

  const std::string& favicon = tab.favicon();
  DVLOG(1) << "Storing synced favicon for url " << navigation_url.spec()
           << " with size " << favicon.size() << " bytes.";
  std::map<std::string, linked_ptr<SyncedFaviconInfo> >::iterator favicon_iter;
  favicon_iter = synced_favicons_.find(favicon_source.spec());
  if (favicon_iter == synced_favicons_.end()) {
    synced_favicons_[favicon_source.spec()] =
        make_linked_ptr<SyncedFaviconInfo>(new SyncedFaviconInfo(favicon));
  } else {
    favicon_iter->second->data = favicon;
    ++favicon_iter->second->usage_count;
  }
  synced_favicon_pages_[navigation_url.spec()] = favicon_source.spec();
}

bool SessionModelAssociator::UpdateSyncModelDataFromClient(
    syncer::SyncError* error) {
  DCHECK(CalledOnValidThread());

  // Associate all open windows and their tabs.
  return AssociateWindows(true, error);
}

SessionModelAssociator::TabNodePool::TabNodePool(
    ProfileSyncService* sync_service)
    : tab_pool_fp_(-1),
      sync_service_(sync_service) {
}

SessionModelAssociator::TabNodePool::~TabNodePool() {}

void SessionModelAssociator::TabNodePool::AddTabNode(int64 sync_id) {
  tab_syncid_pool_.resize(tab_syncid_pool_.size() + 1);
  tab_syncid_pool_[static_cast<size_t>(++tab_pool_fp_)] = sync_id;
}

int64 SessionModelAssociator::TabNodePool::GetFreeTabNode() {
  DCHECK_GT(machine_tag_.length(), 0U);
  if (tab_pool_fp_ == -1) {
    // Tab pool has no free nodes, allocate new one.
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode root(&trans);
    if (root.InitByTagLookup(kSessionsTag) != syncer::BaseNode::INIT_OK) {
      LOG(ERROR) << kNoSessionsFolderError;
      return syncer::kInvalidId;
    }
    size_t tab_node_id = tab_syncid_pool_.size();
    std::string tab_node_tag = TabIdToTag(machine_tag_, tab_node_id);
    syncer::WriteNode tab_node(&trans);
    syncer::WriteNode::InitUniqueByCreationResult result =
        tab_node.InitUniqueByCreation(SESSIONS, root, tab_node_tag);
    if (result != syncer::WriteNode::INIT_SUCCESS) {
      LOG(ERROR) << "Could not create new node with tag "
                 << tab_node_tag << "!";
      return syncer::kInvalidId;
    }
    // We fill the new node with just enough data so that in case of a crash/bug
    // we can identify the node as our own on re-association and reuse it.
    tab_node.SetTitle(UTF8ToWide(tab_node_tag));
    sync_pb::SessionSpecifics specifics;
    specifics.set_session_tag(machine_tag_);
    specifics.set_tab_node_id(tab_node_id);
    tab_node.SetSessionSpecifics(specifics);

    // Grow the pool by 1 since we created a new node. We don't actually need
    // to put the node's id in the pool now, since the pool is still empty.
    // The id will be added when that tab is closed and the node is freed.
    tab_syncid_pool_.resize(tab_node_id + 1);
    DVLOG(1) << "Adding sync node "
             << tab_node.GetId() << " to tab syncid pool";
    return tab_node.GetId();
  } else {
    // There are nodes available, grab next free and decrement free pointer.
    return tab_syncid_pool_[static_cast<size_t>(tab_pool_fp_--)];
  }
}

void SessionModelAssociator::TabNodePool::FreeTabNode(int64 sync_id) {
  // Pool size should always match # of free tab nodes.
  DCHECK_LT(tab_pool_fp_, static_cast<int64>(tab_syncid_pool_.size()));
  tab_syncid_pool_[static_cast<size_t>(++tab_pool_fp_)] = sync_id;
}

void SessionModelAssociator::AttemptSessionsDataRefresh() const {
  DVLOG(1) << "Triggering sync refresh for sessions datatype.";
  const syncer::ModelType type = syncer::SESSIONS;
  syncer::ModelTypeStateMap state_map;
  state_map.insert(std::make_pair(type, syncer::InvalidationState()));
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
      content::Source<Profile>(profile_),
      content::Details<const syncer::ModelTypeStateMap>(&state_map));
}

bool SessionModelAssociator::GetLocalSession(
    const SyncedSession* * local_session) {
  DCHECK(CalledOnValidThread());
  if (current_machine_tag_.empty())
    return false;
  *local_session = synced_session_tracker_.GetSession(GetCurrentMachineTag());
  return true;
}

bool SessionModelAssociator::GetAllForeignSessions(
    std::vector<const SyncedSession*>* sessions) {
  DCHECK(CalledOnValidThread());
  return synced_session_tracker_.LookupAllForeignSessions(sessions);
}

bool SessionModelAssociator::GetForeignSession(
    const std::string& tag,
    std::vector<const SessionWindow*>* windows) {
  DCHECK(CalledOnValidThread());
  return synced_session_tracker_.LookupSessionWindows(tag, windows);
}

bool SessionModelAssociator::GetForeignTab(
    const std::string& tag,
    const SessionID::id_type tab_id,
    const SessionTab** tab) {
  DCHECK(CalledOnValidThread());
  const SyncedSessionTab* synced_tab;
  bool success = synced_session_tracker_.LookupSessionTab(tag,
                                                          tab_id,
                                                          &synced_tab);
  if (success)
    *tab = synced_tab;
  return success;
}

void SessionModelAssociator::DeleteStaleSessions() {
  DCHECK(CalledOnValidThread());
  std::vector<const SyncedSession*> sessions;
  if (!GetAllForeignSessions(&sessions))
    return;  // No foreign sessions.

  // Iterate through all the sessions and delete any with age older than
  // |stale_session_threshold_days_|.
  for (std::vector<const SyncedSession*>::const_iterator iter =
           sessions.begin(); iter != sessions.end(); ++iter) {
    const SyncedSession* session = *iter;
    int session_age_in_days =
        (base::Time::Now() - session->modified_time).InDays();
    std::string session_tag = session->session_tag;
    if (session_age_in_days > 0 &&  // If false, local clock is not trustworty.
        static_cast<size_t>(session_age_in_days) >
            stale_session_threshold_days_) {
      DVLOG(1) << "Found stale session " << session_tag
               << " with age " << session_age_in_days << ", deleting.";
      DeleteForeignSession(session_tag);
    }
  }
}

void SessionModelAssociator::SetStaleSessionThreshold(
    size_t stale_session_threshold_days) {
  DCHECK(CalledOnValidThread());
  if (stale_session_threshold_days_ == 0) {
    NOTREACHED() << "Attempted to set invalid stale session threshold.";
    return;
  }
  stale_session_threshold_days_ = stale_session_threshold_days;
  // TODO(zea): maybe make this preference-based? Might be nice to let users be
  // able to modify this once and forget about it. At the moment, if we want a
  // different threshold we will need to call this everytime we create a new
  // model associator and before we AssociateModels (probably from DTC).
}

void SessionModelAssociator::DeleteForeignSession(const std::string& tag) {
  DCHECK(CalledOnValidThread());
  if (tag == GetCurrentMachineTag()) {
    LOG(ERROR) << "Attempting to delete local session. This is not currently "
               << "supported.";
    return;
  }

  if (!DisassociateForeignSession(tag)) {
    // We don't have any data for this session, our work here is done!
    return;
  }

  syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  syncer::ReadNode root(&trans);
  if (root.InitByTagLookup(kSessionsTag) != syncer::BaseNode::INIT_OK) {
    LOG(ERROR) << kNoSessionsFolderError;
    return;
  }
  int64 id = root.GetFirstChildId();
  while (id != syncer::kInvalidId) {
    syncer::WriteNode sync_node(&trans);
    if (sync_node.InitByIdLookup(id) != syncer::BaseNode::INIT_OK) {
      LOG(ERROR) << "Failed to fetch sync node for id " << id;
      continue;
    }
    id = sync_node.GetSuccessorId();
    const sync_pb::SessionSpecifics& specifics =
        sync_node.GetSessionSpecifics();
    if (specifics.session_tag() == tag)
      sync_node.Remove();
  }
}

bool SessionModelAssociator::IsValidTab(const SyncedTabDelegate& tab) const {
  if ((!sync_service_ || tab.profile() != sync_service_->profile()) &&
      !setup_for_test_) {
    return false;
  }
  const SyncedWindowDelegate* window =
      SyncedWindowDelegate::FindSyncedWindowDelegateWithId(
          tab.GetWindowId());
  if (!window && !setup_for_test_)
    return false;
  return true;
}

bool SessionModelAssociator::TabHasValidEntry(
    const SyncedTabDelegate& tab) const {
  int pending_index = tab.GetPendingEntryIndex();
  int entry_count = tab.GetEntryCount();
  bool found_valid_url = false;
  if (entry_count == 0)
    return false;  // This deliberately ignores a new pending entry.
  for (int i = 0; i < entry_count; ++i) {
    const content::NavigationEntry* entry = (i == pending_index) ?
       tab.GetPendingEntry() : tab.GetEntryAtIndex(i);
    if (!entry)
      return false;
    if (entry->GetVirtualURL().is_valid() &&
        !entry->GetVirtualURL().SchemeIs("chrome") &&
        !entry->GetVirtualURL().SchemeIsFile()) {
      found_valid_url = true;
    }
  }
  return found_valid_url;
}

// If this functionality changes, SyncedSession::ShouldSyncSessionTab should be
// modified to match.
bool SessionModelAssociator::ShouldSyncTab(const SyncedTabDelegate& tab) const {
  DCHECK(CalledOnValidThread());
  if (!IsValidTab(tab))
    return false;
  return TabHasValidEntry(tab);
}

void SessionModelAssociator::QuitLoopForSubtleTesting() {
  if (waiting_for_change_) {
    DVLOG(1) << "Quitting MessageLoop for test.";
    waiting_for_change_ = false;
    test_weak_factory_.InvalidateWeakPtrs();
    MessageLoop::current()->Quit();
  }
}

void SessionModelAssociator::BlockUntilLocalChangeForTest(
    base::TimeDelta timeout) {
  if (test_weak_factory_.HasWeakPtrs())
    return;
  waiting_for_change_ = true;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SessionModelAssociator::QuitLoopForSubtleTesting,
                 test_weak_factory_.GetWeakPtr()),
      timeout);
}

bool SessionModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  const syncer::ModelTypeSet encrypted_types = trans.GetEncryptedTypes();
  return !encrypted_types.Has(SESSIONS) ||
         sync_service_->IsCryptographerReady(&trans);
}

}  // namespace browser_sync
