// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/session_model_associator.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/read_transaction.h"
#include "chrome/browser/sync/internal_api/write_node.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/syncable/syncable.h"
#include "sync/util/get_session_name.h"
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
using syncable::SESSIONS;

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
  return internal::IsTabletUi() ?
      sync_pb::SessionHeader_DeviceType_TYPE_TABLET :
      sync_pb::SessionHeader_DeviceType_TYPE_PHONE;
#else
  return sync_pb::SessionHeader_DeviceType_TYPE_OTHER;
#endif
}

}  // namespace

SessionModelAssociator::SessionModelAssociator(ProfileSyncService* sync_service)
    : tab_pool_(sync_service),
      local_session_syncid_(sync_api::kInvalidId),
      sync_service_(sync_service),
      stale_session_threshold_days_(kDefaultStaleSessionThresholdDays),
      setup_for_test_(false),
      waiting_for_change_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(test_weak_factory_(this)),
      profile_(sync_service->profile()),
      pref_service_(profile_->GetPrefs()) {
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
      local_session_syncid_(sync_api::kInvalidId),
      sync_service_(sync_service),
      stale_session_threshold_days_(kDefaultStaleSessionThresholdDays),
      setup_for_test_(setup_for_test),
      waiting_for_change_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(test_weak_factory_(this)),
      profile_(sync_service->profile()),
      pref_service_(NULL)  {
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
    sync_api::BaseNode* sync_node) {
  NOTREACHED();
  return false;
}

bool SessionModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(CalledOnValidThread());
  CHECK(has_nodes);
  *has_nodes = false;
  sync_api::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(kSessionsTag)) {
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
  sync_api::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  sync_api::ReadNode node(&trans);
  if (!node.InitByClientTagLookup(SESSIONS, tag))
    return sync_api::kInvalidId;
  return node.GetId();
}

const SyncedTabDelegate*
SessionModelAssociator::GetChromeNodeFromSyncId(int64 sync_id) {
  NOTREACHED();
  return NULL;
}

bool SessionModelAssociator::InitSyncNodeFromChromeId(
    const size_t& id,
    sync_api::BaseNode* sync_node) {
  NOTREACHED();
  return false;
}

bool SessionModelAssociator::AssociateWindows(bool reload_tabs,
                                              SyncError* error) {
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
        const SessionTab* tab;
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

  sync_api::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  sync_api::WriteNode header_node(&trans);
  if (!header_node.InitByIdLookup(local_session_syncid_)) {
    error->Reset(FROM_HERE,
                 "Failed to load local session header node.",
                 model_type());
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
    SyncError* error) {
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
                                          SyncError* error) {
  DCHECK(CalledOnValidThread());
  int64 sync_id;
  SessionID::id_type id = tab.GetSessionId();
  if (tab.IsBeingDestroyed()) {
    // This tab is closing.
    TabLinksMap::iterator tab_iter = tab_map_.find(id);
    if (tab_iter == tab_map_.end()) {
      // We aren't tracking this tab (for example, sync setting page).
      return true;
    }
    tab_pool_.FreeTabNode(tab_iter->second.sync_id());
    tab_map_.erase(tab_iter);
    return true;
  }

  if (!ShouldSyncTab(tab))
    return true;

  TabLinksMap::const_iterator tablink = tab_map_.find(id);
  if (tablink == tab_map_.end()) {
    // This is a new tab, get a sync node for it.
    sync_id = tab_pool_.GetFreeTabNode();
    if (sync_id == sync_api::kInvalidId) {
      error->Reset(FROM_HERE,
                   "Received invalid tab node from tab pool. Reassociation "
                       "needed.",
                    model_type());
      return false;
    }
  } else {
    // This tab is already associated with a sync node, reuse it.
    sync_id = tablink->second.sync_id();
  }

  DVLOG(1) << "Reloading tab " << id << " from window " << tab.GetWindowId();
  const SyncedWindowDelegate* window =
      SyncedWindowDelegate::FindSyncedWindowDelegateWithId(
          tab.GetWindowId());
  DCHECK(window);
  TabLinks t(sync_id, &tab);
  tab_map_[id] = t;
  return WriteTabContentsToSyncModel(*window, tab, sync_id, error);
}

bool SessionModelAssociator::WriteTabContentsToSyncModel(
    const SyncedWindowDelegate& window,
    const SyncedTabDelegate& tab,
    int64 sync_id,
    SyncError* error) {
  DCHECK(CalledOnValidThread());
  sync_pb::SessionSpecifics session_s;
  session_s.set_session_tag(GetCurrentMachineTag());
  sync_pb::SessionTab* tab_s = session_s.mutable_tab();

  SessionID::id_type tab_id = tab.GetSessionId();
  tab_s->set_tab_id(tab_id);
  tab_s->set_window_id(tab.GetWindowId());
  const int current_index = tab.GetCurrentEntryIndex();
  const int min_index = std::max(0,
                                 current_index - kMaxSyncNavigationCount);
  const int max_index = std::min(current_index + kMaxSyncNavigationCount,
                                 tab.GetEntryCount());
  const int pending_index = tab.GetPendingEntryIndex();
  tab_s->set_pinned(window.IsTabPinned(&tab));
  if (tab.HasExtensionAppId()) {
    tab_s->set_extension_app_id(tab.GetExtensionAppId());
  }
  for (int i = min_index; i < max_index; ++i) {
    const NavigationEntry* entry = (i == pending_index) ?
       tab.GetPendingEntry() : tab.GetEntryAtIndex(i);
    DCHECK(entry);
    if (entry->GetVirtualURL().is_valid()) {
      if (i == max_index - 1) {
        DVLOG(1) << "Associating tab " << tab_id << " with sync id " << sync_id
                 << ", url " << entry->GetVirtualURL().possibly_invalid_spec()
                 << " and title " << entry->GetTitle();
      }
      TabNavigation tab_nav;
      tab_nav.SetFromNavigationEntry(*entry);
      sync_pb::TabNavigation* nav_s = tab_s->add_navigation();
      PopulateSessionSpecificsNavigation(&tab_nav, nav_s);
    }
  }
  tab_s->set_current_navigation_index(current_index);

  // Convert to a local representation and store in synced session tracker.
  SessionTab* session_tab =
      synced_session_tracker_.GetTab(GetCurrentMachineTag(),
                                     tab_s->tab_id());
  synced_session_tracker_.GetSession(GetCurrentMachineTag())->modified_time =
      base::Time::Now();
  PopulateSessionTabFromSpecifics(*tab_s,
                                  base::Time::Now(),
                                  session_tab);

  // Write into the actual sync model.
  sync_api::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  sync_api::WriteNode tab_node(&trans);
  if (!tab_node.InitByIdLookup(sync_id)) {
    error->Reset(FROM_HERE, "Failed to look up local tab node", model_type());
    return false;
  }
  tab_node.SetSessionSpecifics(session_s);
  return true;
}

// Static
// TODO(zea): perhaps sync state (scroll position, form entries, etc.) as well?
// See http://crbug.com/67068.
void SessionModelAssociator::PopulateSessionSpecificsNavigation(
    const TabNavigation* navigation,
    sync_pb::TabNavigation* tab_navigation) {
  tab_navigation->set_index(navigation->index());
  tab_navigation->set_virtual_url(navigation->virtual_url().spec());
  // FIXME(zea): Support referrer policy?
  tab_navigation->set_referrer(navigation->referrer().url.spec());
  tab_navigation->set_title(UTF16ToUTF8(navigation->title()));
  switch (navigation->transition()) {
    case content::PAGE_TRANSITION_LINK:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_LINK);
      break;
    case content::PAGE_TRANSITION_TYPED:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_TYPED);
      break;
    case content::PAGE_TRANSITION_AUTO_BOOKMARK:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_AUTO_BOOKMARK);
      break;
    case content::PAGE_TRANSITION_AUTO_SUBFRAME:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_AUTO_SUBFRAME);
      break;
    case content::PAGE_TRANSITION_MANUAL_SUBFRAME:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_MANUAL_SUBFRAME);
      break;
    case content::PAGE_TRANSITION_GENERATED:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_GENERATED);
      break;
    case content::PAGE_TRANSITION_START_PAGE:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_START_PAGE);
      break;
    case content::PAGE_TRANSITION_FORM_SUBMIT:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_FORM_SUBMIT);
      break;
    case content::PAGE_TRANSITION_RELOAD:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_RELOAD);
      break;
    case content::PAGE_TRANSITION_KEYWORD:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_KEYWORD);
      break;
    case content::PAGE_TRANSITION_KEYWORD_GENERATED:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_KEYWORD_GENERATED);
      break;
    case content::PAGE_TRANSITION_CHAIN_START:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_CHAIN_START);
      break;
    case content::PAGE_TRANSITION_CHAIN_END:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_CHAIN_END);
      break;
    case content::PAGE_TRANSITION_CLIENT_REDIRECT:
      tab_navigation->set_navigation_qualifier(
        sync_pb::TabNavigation_PageTransitionQualifier_CLIENT_REDIRECT);
      break;
    case content::PAGE_TRANSITION_SERVER_REDIRECT:
      tab_navigation->set_navigation_qualifier(
        sync_pb::TabNavigation_PageTransitionQualifier_SERVER_REDIRECT);
      break;
    default:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_TYPED);
  }
}

void SessionModelAssociator::Associate(const SyncedTabDelegate* tab,
                                       int64 sync_id) {
  NOTIMPLEMENTED();
}

void SessionModelAssociator::Disassociate(int64 sync_id) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

bool SessionModelAssociator::AssociateModels(SyncError* error) {
  DCHECK(CalledOnValidThread());

  // Ensure that we disassociated properly, otherwise memory might leak.
  DCHECK(synced_session_tracker_.Empty());
  DCHECK_EQ(0U, tab_pool_.capacity());

  local_session_syncid_ = sync_api::kInvalidId;

  // Read any available foreign sessions and load any session data we may have.
  // If we don't have any local session data in the db, create a header node.
  {
    sync_api::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());

    sync_api::ReadNode root(&trans);
    if (!root.InitByTagLookup(syncable::ModelTypeToRootTag(model_type()))) {
      error->Reset(FROM_HERE, kNoSessionsFolderError, model_type());
      return false;
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
    if (!UpdateAssociationsFromSyncModel(root, &trans, error))
      return false;

    if (local_session_syncid_ == sync_api::kInvalidId) {
      // The sync db didn't have a header node for us, we need to create one.
      sync_api::WriteNode write_node(&trans);
      if (!write_node.InitUniqueByCreation(SESSIONS, root,
                                           current_machine_tag_)) {
        error->Reset(FROM_HERE,
                     "Failed to create sessions header sync node.",
                     model_type());
        return false;
      }
      write_node.SetTitle(UTF8ToWide(current_machine_tag_));
      local_session_syncid_ = write_node.GetId();
    }
  }

  // Check if anything has changed on the client side.
  if (!UpdateSyncModelDataFromClient(error))
    return false;

  DVLOG(1) << "Session models associated.";
  return true;
}

bool SessionModelAssociator::DisassociateModels(SyncError* error) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Disassociating local session " << GetCurrentMachineTag();
  synced_session_tracker_.Clear();
  tab_map_.clear();
  tab_pool_.clear();
  local_session_syncid_ = sync_api::kInvalidId;
  current_machine_tag_ = "";
  current_session_name_ = "";

  // There is no local model stored with which to disassociate, just notify
  // foreign session handlers.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FOREIGN_SESSION_DISABLED,
      content::Source<Profile>(sync_service_->profile()),
      content::NotificationService::NoDetails());
  return true;
}

void SessionModelAssociator::InitializeCurrentMachineTag(
    sync_api::WriteTransaction* trans) {
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
    syncable::Directory* dir = trans->GetWrappedWriteTrans()->directory();
    current_machine_tag_ = "session_sync";
    current_machine_tag_.append(dir->cache_guid());
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
  if (current_session_name_.empty())
    current_session_name_ = name;
}

void SessionModelAssociator::InitializeCurrentSessionName() {
  DCHECK(CalledOnValidThread());
  if (setup_for_test_) {
    OnSessionNameInitialized("TestSessionName");
  } else {
    browser_sync::GetSessionName(
        BrowserThread::GetBlockingPool(),
        base::Bind(&SessionModelAssociator::OnSessionNameInitialized,
                   AsWeakPtr()));
  }
}

bool SessionModelAssociator::UpdateAssociationsFromSyncModel(
    const sync_api::ReadNode& root,
    sync_api::WriteTransaction* trans,
    SyncError* error) {
  DCHECK(CalledOnValidThread());
  DCHECK(tab_pool_.empty());
  DCHECK_EQ(local_session_syncid_, sync_api::kInvalidId);

  // Iterate through the nodes and associate any foreign sessions.
  int64 id = root.GetFirstChildId();
  while (id != sync_api::kInvalidId) {
    sync_api::WriteNode sync_node(trans);
    if (!sync_node.InitByIdLookup(id)) {
      error->Reset(FROM_HERE, "Failed to load sync node", model_type());
      return false;
    }
    int64 next_id = sync_node.GetSuccessorId();

    const sync_pb::SessionSpecifics& specifics =
        sync_node.GetSessionSpecifics();
    const base::Time& modification_time = sync_node.GetModificationTime();
    if (specifics.session_tag() != GetCurrentMachineTag()) {
      AssociateForeignSpecifics(specifics, modification_time);
    } else {
      // This is previously stored local session information.
      if (specifics.has_header() &&
          local_session_syncid_ == sync_api::kInvalidId) {
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
    SessionTab* tab =
        synced_session_tracker_.GetTab(foreign_session_tag, tab_id);
    PopulateSessionTabFromSpecifics(tab_s, modification_time, tab);
    if (foreign_session->modified_time < modification_time)
      foreign_session->modified_time = modification_time;
  } else {
    LOG(WARNING) << "Ignoring foreign session node with missing header/tab "
                 << "fields and tag " << foreign_session_tag << ".";
  }
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
    SessionTab* tab) {
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
  tab->navigations.clear();  // In case we are reusing a previous SessionTab.
  for (int i = 0; i < specifics.navigation_size(); i++) {
    AppendSessionTabNavigation(specifics.navigation(i), &tab->navigations);
  }
}

// Static
void SessionModelAssociator::AppendSessionTabNavigation(
    const sync_pb::TabNavigation& specifics,
    std::vector<TabNavigation>* navigations) {
  int index = 0;
  GURL virtual_url;
  GURL referrer;
  string16 title;
  std::string state;
  content::PageTransition transition(content::PAGE_TRANSITION_LINK);
  if (specifics.has_index())
    index = specifics.index();
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
      case sync_pb::TabNavigation_PageTransition_LINK:
        transition = content::PAGE_TRANSITION_LINK;
        break;
      case sync_pb::TabNavigation_PageTransition_TYPED:
        transition = content::PAGE_TRANSITION_TYPED;
        break;
      case sync_pb::TabNavigation_PageTransition_AUTO_BOOKMARK:
        transition = content::PAGE_TRANSITION_AUTO_BOOKMARK;
        break;
      case sync_pb::TabNavigation_PageTransition_AUTO_SUBFRAME:
        transition = content::PAGE_TRANSITION_AUTO_SUBFRAME;
        break;
      case sync_pb::TabNavigation_PageTransition_MANUAL_SUBFRAME:
        transition = content::PAGE_TRANSITION_MANUAL_SUBFRAME;
        break;
      case sync_pb::TabNavigation_PageTransition_GENERATED:
        transition = content::PAGE_TRANSITION_GENERATED;
        break;
      case sync_pb::TabNavigation_PageTransition_START_PAGE:
        transition = content::PAGE_TRANSITION_START_PAGE;
        break;
      case sync_pb::TabNavigation_PageTransition_FORM_SUBMIT:
        transition = content::PAGE_TRANSITION_FORM_SUBMIT;
        break;
      case sync_pb::TabNavigation_PageTransition_RELOAD:
        transition = content::PAGE_TRANSITION_RELOAD;
        break;
      case sync_pb::TabNavigation_PageTransition_KEYWORD:
        transition = content::PAGE_TRANSITION_KEYWORD;
        break;
      case sync_pb::TabNavigation_PageTransition_KEYWORD_GENERATED:
        transition = content::PAGE_TRANSITION_KEYWORD_GENERATED;
        break;
      case sync_pb::TabNavigation_PageTransition_CHAIN_START:
        transition = content::PAGE_TRANSITION_CHAIN_START;
        break;
      case sync_pb::TabNavigation_PageTransition_CHAIN_END:
        transition = content::PAGE_TRANSITION_CHAIN_END;
        break;
      default:
        switch (specifics.navigation_qualifier()) {
          case sync_pb::
              TabNavigation_PageTransitionQualifier_CLIENT_REDIRECT:
            transition = content::PAGE_TRANSITION_CLIENT_REDIRECT;
            break;
            case sync_pb::
                TabNavigation_PageTransitionQualifier_SERVER_REDIRECT:
            transition = content::PAGE_TRANSITION_SERVER_REDIRECT;
              break;
            default:
            transition = content::PAGE_TRANSITION_TYPED;
        }
    }
  }
  TabNavigation tab_navigation(
      index, virtual_url,
      content::Referrer(referrer, WebKit::WebReferrerPolicyDefault), title,
      state, transition);
  navigations->insert(navigations->end(), tab_navigation);
}

bool SessionModelAssociator::UpdateSyncModelDataFromClient(SyncError* error) {
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
    sync_api::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    sync_api::ReadNode root(&trans);
    if (!root.InitByTagLookup(kSessionsTag)) {
      LOG(ERROR) << kNoSessionsFolderError;
      return sync_api::kInvalidId;
    }
    size_t tab_node_id = tab_syncid_pool_.size();
    std::string tab_node_tag = TabIdToTag(machine_tag_, tab_node_id);
    sync_api::WriteNode tab_node(&trans);
    if (!tab_node.InitUniqueByCreation(SESSIONS, root, tab_node_tag)) {
      LOG(ERROR) << "Could not create new node with tag "
                 << tab_node_tag << "!";
      return sync_api::kInvalidId;
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
  const syncable::ModelType type = syncable::SESSIONS;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SYNC_REFRESH,
      content::Source<Profile>(profile_),
      content::Details<const syncable::ModelType>(&type));
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
  return synced_session_tracker_.LookupSessionTab(tag, tab_id, tab);
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

  sync_api::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(kSessionsTag)) {
    LOG(ERROR) << kNoSessionsFolderError;
    return;
  }
  int64 id = root.GetFirstChildId();
  while (id != sync_api::kInvalidId) {
    sync_api::WriteNode sync_node(&trans);
    if (!sync_node.InitByIdLookup(id)) {
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
    int64 timeout_milliseconds) {
  if (test_weak_factory_.HasWeakPtrs())
    return;
  waiting_for_change_ = true;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SessionModelAssociator::QuitLoopForSubtleTesting,
                 test_weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(timeout_milliseconds));
}

bool SessionModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  sync_api::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  const syncable::ModelTypeSet encrypted_types =
      sync_api::GetEncryptedTypes(&trans);
  return !encrypted_types.Has(SESSIONS) ||
         sync_service_->IsCryptographerReady(&trans);
}

}  // namespace browser_sync
