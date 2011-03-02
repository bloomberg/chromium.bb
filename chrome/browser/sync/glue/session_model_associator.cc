// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/session_model_associator.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"

namespace browser_sync {

namespace {
static const char kNoSessionsFolderError[] =
    "Server did not create the top-level sessions node. We "
    "might be running against an out-of-date server.";

// The maximum number of navigations in each direction we care to sync.
static const int max_sync_navigation_count = 6;
}  // namespace

SessionModelAssociator::SessionModelAssociator(ProfileSyncService* sync_service)
    : tab_pool_(sync_service),
      local_session_syncid_(sync_api::kInvalidId),
      sync_service_(sync_service) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_service_);
}

SessionModelAssociator::SessionModelAssociator(ProfileSyncService* sync_service,
                                               bool setup_for_test)
    : tab_pool_(sync_service),
      local_session_syncid_(sync_api::kInvalidId),
      sync_service_(sync_service),
      setup_for_test_(setup_for_test) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_service_);
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
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(kSessionsTag)) {
    LOG(ERROR) << kNoSessionsFolderError;
    return false;
  }
  // The sync model has user created nodes iff the sessions folder has
  // any children.
  *has_nodes = root.GetFirstChildId() != sync_api::kInvalidId;
  return true;
}

int64 SessionModelAssociator::GetSyncIdFromChromeId(const size_t& id) {
  DCHECK(CalledOnValidThread());
  return GetSyncIdFromSessionTag(TabIdToTag(GetCurrentMachineTag(), id));
}

int64 SessionModelAssociator::GetSyncIdFromSessionTag(const std::string& tag) {
  DCHECK(CalledOnValidThread());
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());
  sync_api::ReadNode node(&trans);
  if (!node.InitByClientTagLookup(syncable::SESSIONS, tag))
    return sync_api::kInvalidId;
  return node.GetId();
}

const TabContents*
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

void SessionModelAssociator::ReassociateWindows(bool reload_tabs) {
  DCHECK(CalledOnValidThread());
  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(GetCurrentMachineTag());
  sync_pb::SessionHeader* header_s = specifics.mutable_header();

  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    // Make sure the browser has tabs and a window. Browsers destructor
    // removes itself from the BrowserList. When a browser is closed the
    // destructor is not necessarily run immediately. This means its possible
    // for us to get a handle to a browser that is about to be removed. If
    // the tab count is 0 or the window is NULL, the browser is about to be
    // deleted, so we ignore it.
    if (ShouldSyncWindowType((*i)->type()) && (*i)->tab_count() &&
        (*i)->window()) {
      sync_pb::SessionWindow window_s;
      SessionID::id_type window_id = (*i)->session_id().id();
      VLOG(1) << "Reassociating window " << window_id << " with " <<
          (*i)->tab_count() << " tabs.";
      window_s.set_window_id(window_id);
      window_s.set_selected_tab_index((*i)->selected_index());
      if ((*i)->type() ==
          Browser::TYPE_NORMAL) {
        window_s.set_browser_type(
            sync_pb::SessionWindow_BrowserType_TYPE_NORMAL);
      } else {
        window_s.set_browser_type(
            sync_pb::SessionWindow_BrowserType_TYPE_POPUP);
      }

      // Store the order of tabs.
      bool found_tabs = false;
      for (int j = 0; j < (*i)->tab_count(); ++j) {
        TabContents* tab = (*i)->GetTabContentsAt(j);
        DCHECK(tab);
        if (IsValidTab(*tab)) {
          found_tabs = true;
          window_s.add_tab(tab->controller().session_id().id());
          if (reload_tabs) {
            ReassociateTab(*tab);
          }
        }
      }
      // Only add a window if it contains valid tabs.
      if (found_tabs) {
        sync_pb::SessionWindow* header_window = header_s->add_window();
        *header_window = window_s;
      }
    }
  }

  sync_api::WriteTransaction trans(sync_service_->GetUserShare());
  sync_api::WriteNode header_node(&trans);
  if (!header_node.InitByIdLookup(local_session_syncid_)) {
    LOG(ERROR) << "Failed to load local session header node.";
    return;
  }
  header_node.SetSessionSpecifics(specifics);
}

// Static.
bool SessionModelAssociator::ShouldSyncWindowType(const Browser::Type& type) {
  switch (type) {
    case Browser::TYPE_POPUP:
      return true;
    case Browser::TYPE_APP:
      return false;
    case Browser::TYPE_APP_POPUP:
      return false;
    case Browser::TYPE_DEVTOOLS:
      return false;
    case Browser::TYPE_APP_PANEL:
      return false;
    case Browser::TYPE_NORMAL:
    default:
      return true;
  }
}

void SessionModelAssociator::ReassociateTabs(
    const std::vector<TabContents*>& tabs) {
  DCHECK(CalledOnValidThread());
  for (std::vector<TabContents*>::const_iterator i = tabs.begin();
       i != tabs.end();
       ++i) {
    ReassociateTab(**i);
  }
}

void SessionModelAssociator::ReassociateTab(const TabContents& tab) {
  DCHECK(CalledOnValidThread());
  if (!IsValidTab(tab))
    return;

  int64 sync_id;
  SessionID::id_type id = tab.controller().session_id().id();
  if (tab.is_being_destroyed()) {
    // This tab is closing.
    TabLinksMap::iterator tab_iter = tab_map_.find(id);
    if (tab_iter == tab_map_.end()) {
      // We aren't tracking this tab (for example, sync setting page).
      return;
    }
    tab_pool_.FreeTabNode(tab_iter->second.sync_id());
    tab_map_.erase(tab_iter);
    return;
  }

  TabLinksMap::const_iterator tablink = tab_map_.find(id);
  if (tablink == tab_map_.end()) {
    // This is a new tab, get a sync node for it.
    sync_id = tab_pool_.GetFreeTabNode();
  } else {
    // This tab is already associated with a sync node, reuse it.
    sync_id = tablink->second.sync_id();
  }
  Associate(&tab, sync_id);
}

void SessionModelAssociator::Associate(const TabContents* tab, int64 sync_id) {
  DCHECK(CalledOnValidThread());
  SessionID::id_type session_id = tab->controller().session_id().id();

  TabLinks t(sync_id, tab);
  tab_map_[session_id] = t;

  sync_api::WriteTransaction trans(sync_service_->GetUserShare());
  WriteTabContentsToSyncModel(*tab, sync_id, &trans);
}

bool SessionModelAssociator::WriteTabContentsToSyncModel(
    const TabContents& tab,
    int64 sync_id,
    sync_api::WriteTransaction* trans) {
  DCHECK(CalledOnValidThread());
  sync_api::WriteNode tab_node(trans);
  if (!tab_node.InitByIdLookup(sync_id)) {
    LOG(ERROR) << "Failed to look up tab node " << sync_id;
    return false;
  }

  sync_pb::SessionSpecifics session_s;
  session_s.set_session_tag(GetCurrentMachineTag());
  sync_pb::SessionTab* tab_s = session_s.mutable_tab();

  SessionID::id_type tab_id = tab.controller().session_id().id();
  tab_s->set_tab_id(tab_id);
  tab_s->set_window_id(tab.controller().window_id().id());
  const int current_index = tab.controller().GetCurrentEntryIndex();
  const int min_index = std::max(0,
                                 current_index - max_sync_navigation_count);
  const int max_index = std::min(current_index + max_sync_navigation_count,
                                 tab.controller().entry_count());
  const int pending_index = tab.controller().pending_entry_index();
  Browser* browser = BrowserList::FindBrowserWithID(
      tab.controller().window_id().id());
  DCHECK(browser);
  int index_in_window = browser->tabstrip_model()->GetWrapperIndex(&tab);
  DCHECK(index_in_window != TabStripModel::kNoTab);
  tab_s->set_pinned(browser->tabstrip_model()->IsTabPinned(index_in_window));
  if (tab.extension_app())
    tab_s->set_extension_app_id(tab.extension_app()->id());
  for (int i = min_index; i < max_index; ++i) {
    const NavigationEntry* entry = (i == pending_index) ?
       tab.controller().pending_entry() : tab.controller().GetEntryAtIndex(i);
    DCHECK(entry);
    if (entry->virtual_url().is_valid()) {
      if (i == max_index - 1) {
        VLOG(1) << "Associating tab " << tab_id << " with sync id " << sync_id
            << " and url " << entry->virtual_url().possibly_invalid_spec();
      }
      TabNavigation tab_nav;
      tab_nav.SetFromNavigationEntry(*entry);
      sync_pb::TabNavigation* nav_s = tab_s->add_navigation();
      PopulateSessionSpecificsNavigation(&tab_nav, nav_s);
    }
  }
  tab_s->set_current_navigation_index(current_index);

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
  tab_navigation->set_referrer(navigation->referrer().spec());
  tab_navigation->set_title(UTF16ToUTF8(navigation->title()));
  switch (navigation->transition()) {
    case PageTransition::LINK:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_LINK);
      break;
    case PageTransition::TYPED:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_TYPED);
      break;
    case PageTransition::AUTO_BOOKMARK:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_AUTO_BOOKMARK);
      break;
    case PageTransition::AUTO_SUBFRAME:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_AUTO_SUBFRAME);
      break;
    case PageTransition::MANUAL_SUBFRAME:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_MANUAL_SUBFRAME);
      break;
    case PageTransition::GENERATED:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_GENERATED);
      break;
    case PageTransition::START_PAGE:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_START_PAGE);
      break;
    case PageTransition::FORM_SUBMIT:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_FORM_SUBMIT);
      break;
    case PageTransition::RELOAD:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_RELOAD);
      break;
    case PageTransition::KEYWORD:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_KEYWORD);
      break;
    case PageTransition::KEYWORD_GENERATED:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_KEYWORD_GENERATED);
      break;
    case PageTransition::CHAIN_START:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_CHAIN_START);
      break;
    case PageTransition::CHAIN_END:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_CHAIN_END);
      break;
    case PageTransition::CLIENT_REDIRECT:
      tab_navigation->set_navigation_qualifier(
        sync_pb::TabNavigation_PageTransitionQualifier_CLIENT_REDIRECT);
      break;
    case PageTransition::SERVER_REDIRECT:
      tab_navigation->set_navigation_qualifier(
        sync_pb::TabNavigation_PageTransitionQualifier_SERVER_REDIRECT);
      break;
    default:
      tab_navigation->set_page_transition(
        sync_pb::TabNavigation_PageTransition_TYPED);
  }
}

void SessionModelAssociator::Disassociate(int64 sync_id) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  // TODO(zea): we will need this once we support deleting foreign sessions.
}

bool SessionModelAssociator::AssociateModels() {
  DCHECK(CalledOnValidThread());

  // Ensure that we disassociated properly, otherwise memory might leak.
  DCHECK(foreign_session_tracker_.empty());
  DCHECK_EQ(0U, tab_pool_.capacity());

  local_session_syncid_ = sync_api::kInvalidId;

  // Read any available foreign sessions and load any session data we may have.
  // If we don't have any local session data in the db, create a header node.
  {
    sync_api::WriteTransaction trans(sync_service_->GetUserShare());

    sync_api::ReadNode root(&trans);
    if (!root.InitByTagLookup(kSessionsTag)) {
      LOG(ERROR) << kNoSessionsFolderError;
      return false;
    }

    // Make sure we have a machine tag.
    if (current_machine_tag_.empty())
      InitializeCurrentMachineTag(&trans);

    UpdateAssociationsFromSyncModel(root, &trans);

    if (local_session_syncid_ == sync_api::kInvalidId) {
      // The sync db didn't have a header node for us, we need to create one.
      sync_api::WriteNode write_node(&trans);
      if (!write_node.InitUniqueByCreation(syncable::SESSIONS, root,
          current_machine_tag_)) {
        LOG(ERROR) << "Failed to create sessions header sync node.";
        return false;
      }
      write_node.SetTitle(UTF8ToWide(current_machine_tag_));
      local_session_syncid_ = write_node.GetId();
    }
  }

  // Check if anything has changed on the client side.
  UpdateSyncModelDataFromClient();

  VLOG(1) << "Session models associated.";

  return true;
}

bool SessionModelAssociator::DisassociateModels() {
  DCHECK(CalledOnValidThread());
  foreign_session_tracker_.clear();
  tab_map_.clear();
  tab_pool_.clear();
  local_session_syncid_ = sync_api::kInvalidId;

  // There is no local model stored with which to disassociate, just notify
  // foreign session handlers.
  NotificationService::current()->Notify(
      NotificationType::FOREIGN_SESSION_DISABLED,
      NotificationService::AllSources(),
      NotificationService::NoDetails());
  return true;
}

void SessionModelAssociator::InitializeCurrentMachineTag(
    sync_api::WriteTransaction* trans) {
  DCHECK(CalledOnValidThread());
  syncable::Directory* dir = trans->GetWrappedWriteTrans()->directory();

  // TODO(zea): We need a better way of creating a machine tag. The directory
  // kernel's cache_guid changes every time syncing is turned on and off. This
  // will result in session's associated with stale machine tags persisting on
  // the server since that tag will not be reused. Eventually this should
  // become some string identifiable to the user. (Home, Work, Laptop, etc.)
  // See issue at http://crbug.com/59672
  current_machine_tag_ = "session_sync";
  current_machine_tag_.append(dir->cache_guid());
  VLOG(1) << "Creating machine tag: " << current_machine_tag_;
  tab_pool_.set_machine_tag(current_machine_tag_);
}

bool SessionModelAssociator::UpdateAssociationsFromSyncModel(
    const sync_api::ReadNode& root,
    const sync_api::BaseTransaction* trans) {
  DCHECK(CalledOnValidThread());

  // Iterate through the nodes and associate any foreign sessions.
  int64 id = root.GetFirstChildId();
  while (id != sync_api::kInvalidId) {
    sync_api::ReadNode sync_node(trans);
    if (!sync_node.InitByIdLookup(id)) {
      LOG(ERROR) << "Failed to fetch sync node for id " << id;
      return false;
    }

    const sync_pb::SessionSpecifics& specifics =
        sync_node.GetSessionSpecifics();
    const int64 modification_time = sync_node.GetModificationTime();
    if (specifics.session_tag() != GetCurrentMachineTag()) {
      if (!AssociateForeignSpecifics(specifics, modification_time)) {
        return false;
      }
    } else if (id != local_session_syncid_) {
      // This is previously stored local session information.
      if (specifics.has_header()) {
        DCHECK_EQ(sync_api::kInvalidId, local_session_syncid_);

        // This is our previous header node, reuse it.
        local_session_syncid_ = id;
      } else {
        DCHECK(specifics.has_tab());

        // This is a tab node. We want to track these to reuse them in our free
        // tab node pool. They will be overwritten eventually, so need to do
        // anything else.
        tab_pool_.AddTabNode(id);
      }
    }

    id = sync_node.GetSuccessorId();
  }

  // After updating from sync model all tabid's should be free.
  DCHECK(tab_pool_.full());

  return true;
}

bool SessionModelAssociator::AssociateForeignSpecifics(
    const sync_pb::SessionSpecifics& specifics,
    const int64 modification_time) {
  DCHECK(CalledOnValidThread());
  std::string foreign_session_tag = specifics.session_tag();
  DCHECK(foreign_session_tag != GetCurrentMachineTag() || setup_for_test_);

  if (specifics.has_header()) {
    // Read in the header data for this foreign session.
    // Header data contains window information and ordered tab id's for each
    // window.

    // Load (or create) the ForeignSession object for this client.
    ForeignSession* foreign_session =
        foreign_session_tracker_.GetForeignSession(foreign_session_tag);

    const sync_pb::SessionHeader& header = specifics.header();
    foreign_session->windows.reserve(header.window_size());
    VLOG(1) << "Associating " << foreign_session_tag << " with " <<
        header.window_size() << " windows.";
    size_t i;
    for (i = 0; i < static_cast<size_t>(header.window_size()); ++i) {
      if (i >= foreign_session->windows.size()) {
        // This a new window, create it.
        foreign_session->windows.push_back(new SessionWindow());
      }
      const sync_pb::SessionWindow& window_s = header.window(i);
      PopulateSessionWindowFromSpecifics(foreign_session_tag,
                                         window_s,
                                         modification_time,
                                         foreign_session->windows[i],
                                         &foreign_session_tracker_);
    }
    // Remove any remaining windows (in case windows were closed)
    for (; i < foreign_session->windows.size(); ++i) {
      delete foreign_session->windows[i];
    }
    foreign_session->windows.resize(header.window_size());
  } else if (specifics.has_tab()) {
    const sync_pb::SessionTab& tab_s = specifics.tab();
    SessionID::id_type tab_id = tab_s.tab_id();
    SessionTab* tab =
        foreign_session_tracker_.GetSessionTab(foreign_session_tag,
                                               tab_id,
                                               false);
    PopulateSessionTabFromSpecifics(tab_s, modification_time, tab);
  } else {
    NOTREACHED();
    return false;
  }

  return true;
}

void SessionModelAssociator::DisassociateForeignSession(
    const std::string& foreign_session_tag) {
  DCHECK(CalledOnValidThread());
  foreign_session_tracker_.DeleteForeignSession(foreign_session_tag);
}

// Static
void SessionModelAssociator::PopulateSessionWindowFromSpecifics(
    std::string foreign_session_tag,
    const sync_pb::SessionWindow& specifics,
    int64 mtime,
    SessionWindow* session_window,
    ForeignSessionTracker* tracker) {
  if (specifics.has_window_id())
    session_window->window_id.set_id(specifics.window_id());
  if (specifics.has_selected_tab_index())
    session_window->selected_tab_index = specifics.selected_tab_index();
  if (specifics.has_browser_type()) {
    if (specifics.browser_type() ==
        sync_pb::SessionWindow_BrowserType_TYPE_NORMAL) {
      session_window->type = 1;
    } else {
      session_window->type = 2;
    }
  }
  session_window->timestamp = base::Time::FromInternalValue(mtime);
  session_window->tabs.resize(specifics.tab_size());
  for (int i = 0; i < specifics.tab_size(); i++) {
    SessionID::id_type tab_id = specifics.tab(i);
    session_window->tabs[i] =
        tracker->GetSessionTab(foreign_session_tag, tab_id, true);
  }
}

// Static
void SessionModelAssociator::PopulateSessionTabFromSpecifics(
    const sync_pb::SessionTab& specifics,
    const int64 mtime,
    SessionTab* tab) {
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
  tab->timestamp = base::Time::FromInternalValue(mtime);
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
  PageTransition::Type transition(PageTransition::LINK);
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
        transition = PageTransition::LINK;
        break;
      case sync_pb::TabNavigation_PageTransition_TYPED:
        transition = PageTransition::TYPED;
        break;
      case sync_pb::TabNavigation_PageTransition_AUTO_BOOKMARK:
        transition = PageTransition::AUTO_BOOKMARK;
        break;
      case sync_pb::TabNavigation_PageTransition_AUTO_SUBFRAME:
        transition = PageTransition::AUTO_SUBFRAME;
        break;
      case sync_pb::TabNavigation_PageTransition_MANUAL_SUBFRAME:
        transition = PageTransition::MANUAL_SUBFRAME;
        break;
      case sync_pb::TabNavigation_PageTransition_GENERATED:
        transition = PageTransition::GENERATED;
        break;
      case sync_pb::TabNavigation_PageTransition_START_PAGE:
        transition = PageTransition::START_PAGE;
        break;
      case sync_pb::TabNavigation_PageTransition_FORM_SUBMIT:
        transition = PageTransition::FORM_SUBMIT;
        break;
      case sync_pb::TabNavigation_PageTransition_RELOAD:
        transition = PageTransition::RELOAD;
        break;
      case sync_pb::TabNavigation_PageTransition_KEYWORD:
        transition = PageTransition::KEYWORD;
        break;
      case sync_pb::TabNavigation_PageTransition_KEYWORD_GENERATED:
        transition = PageTransition::KEYWORD_GENERATED;
        break;
      case sync_pb::TabNavigation_PageTransition_CHAIN_START:
        transition = sync_pb::TabNavigation_PageTransition_CHAIN_START;
        break;
      case sync_pb::TabNavigation_PageTransition_CHAIN_END:
        transition = PageTransition::CHAIN_END;
        break;
      default:
        switch (specifics.navigation_qualifier()) {
          case sync_pb::
              TabNavigation_PageTransitionQualifier_CLIENT_REDIRECT:
            transition = PageTransition::CLIENT_REDIRECT;
            break;
            case sync_pb::
                TabNavigation_PageTransitionQualifier_SERVER_REDIRECT:
            transition = PageTransition::SERVER_REDIRECT;
              break;
            default:
            transition = PageTransition::TYPED;
        }
    }
  }
  TabNavigation tab_navigation(index, virtual_url, referrer, title, state,
                               transition);
  navigations->insert(navigations->end(), tab_navigation);
}

void SessionModelAssociator::UpdateSyncModelDataFromClient() {
  DCHECK(CalledOnValidThread());
  // TODO(zea): the logic for determining if we want to sync and the loading of
  // the previous session should go here. We can probably reuse the code for
  // loading the current session from the old session implementation.
  // SessionService::SessionCallback* callback =
  //     NewCallback(this, &SessionModelAssociator::OnGotSession);
  // GetSessionService()->GetCurrentSession(&consumer_, callback);

  // Associate all open windows and their tabs.
  ReassociateWindows(true);
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
    sync_api::WriteTransaction trans(sync_service_->GetUserShare());
    sync_api::ReadNode root(&trans);
    if (!root.InitByTagLookup(kSessionsTag)) {
      LOG(ERROR) << kNoSessionsFolderError;
      return 0;
    }
    size_t tab_node_id = tab_syncid_pool_.size();
    std::string tab_node_tag = TabIdToTag(machine_tag_, tab_node_id);
    sync_api::WriteNode tab_node(&trans);
    if (!tab_node.InitUniqueByCreation(syncable::SESSIONS, root,
                                       tab_node_tag)) {
      LOG(ERROR) << "Could not create new node!";
      return -1;
    }
    tab_node.SetTitle(UTF8ToWide(tab_node_tag));

    // Grow the pool by 1 since we created a new node. We don't actually need
    // to put the node's id in the pool now, since the pool is still empty.
    // The id will be added when that tab is closed and the node is freed.
    tab_syncid_pool_.resize(tab_node_id + 1);
    VLOG(1) << "Adding sync node " << tab_node.GetId() << " to tab syncid pool";
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

bool SessionModelAssociator::GetAllForeignSessions(
    std::vector<const ForeignSession*>* sessions) {
  DCHECK(CalledOnValidThread());
  return foreign_session_tracker_.LookupAllForeignSessions(sessions);
}

bool SessionModelAssociator::GetForeignSession(
    const std::string& tag,
    std::vector<SessionWindow*>* windows) {
  DCHECK(CalledOnValidThread());
  return foreign_session_tracker_.LookupSessionWindows(tag, windows);
}

bool SessionModelAssociator::GetForeignTab(
    const std::string& tag,
    const SessionID::id_type tab_id,
    const SessionTab** tab) {
  DCHECK(CalledOnValidThread());
  return foreign_session_tracker_.LookupSessionTab(tag, tab_id, tab);
}

// Static
bool SessionModelAssociator::SessionWindowHasNoTabsToSync(
    const SessionWindow& window) {
  int num_populated = 0;
  for (std::vector<SessionTab*>::const_iterator i = window.tabs.begin();
      i != window.tabs.end(); ++i) {
    const SessionTab* tab = *i;
    if (IsValidSessionTab(*tab))
      num_populated++;
  }
  if (num_populated == 0)
    return true;
  return false;
}

// Valid local tab?
bool SessionModelAssociator::IsValidTab(const TabContents& tab) {
  DCHECK(CalledOnValidThread());
  if ((tab.profile() == sync_service_->profile() ||
       sync_service_->profile() == NULL)) {
    const NavigationEntry* entry = tab.controller().GetActiveEntry();
    if (!entry)
      return false;
    if (entry->virtual_url().is_valid() &&
        (entry->virtual_url() != GURL(chrome::kChromeUINewTabURL) ||
         tab.controller().entry_count() > 1)) {
      return true;
    }
  }
  return false;
}

// Static
bool SessionModelAssociator::IsValidSessionTab(const SessionTab& tab) {
  if (tab.navigations.empty())
    return false;
  int selected_index = tab.current_navigation_index;
  selected_index = std::max(
      0,
      std::min(selected_index,
          static_cast<int>(tab.navigations.size() - 1)));
  if (selected_index == 0 &&
      tab.navigations.size() == 1 &&
      tab.navigations.at(selected_index).virtual_url() ==
          GURL(chrome::kChromeUINewTabURL)) {
    // This is a new tab with no further history, skip.
    return false;
  }
  return true;
}

// ==========================================================================
// The following methods are not currently used but will likely become useful
// if we choose to sync the previous browser session.

SessionService* SessionModelAssociator::GetSessionService() {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_service_);
  Profile* profile = sync_service_->profile();
  DCHECK(profile);
  SessionService* sessions_service = profile->GetSessionService();
  DCHECK(sessions_service);
  return sessions_service;
}

void SessionModelAssociator::OnGotSession(
    int handle,
    std::vector<SessionWindow*>* windows) {
  DCHECK(CalledOnValidThread());
  DCHECK(local_session_syncid_);

  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(GetCurrentMachineTag());
  sync_pb::SessionHeader* header_s = specifics.mutable_header();
  PopulateSessionSpecificsHeader(*windows, header_s);

  sync_api::WriteTransaction trans(sync_service_->GetUserShare());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(kSessionsTag)) {
    LOG(ERROR) << kNoSessionsFolderError;
    return;
  }

  sync_api::WriteNode header_node(&trans);
  if (!header_node.InitByIdLookup(local_session_syncid_)) {
    LOG(ERROR) << "Failed to load local session header node.";
    return;
  }

  header_node.SetSessionSpecifics(specifics);
}

void SessionModelAssociator::PopulateSessionSpecificsHeader(
    const std::vector<SessionWindow*>& windows,
    sync_pb::SessionHeader* header_s) {
  DCHECK(CalledOnValidThread());

  // Iterate through the vector of windows, extracting the window data, along
  // with the tab data to populate the session specifics.
  for (size_t i = 0; i < windows.size(); ++i) {
    if (SessionWindowHasNoTabsToSync(*(windows[i])))
      continue;
    sync_pb::SessionWindow* window_s = header_s->add_window();
    PopulateSessionSpecificsWindow(*(windows[i]), window_s);
    if (!SyncLocalWindowToSyncModel(*(windows[i])))
      return;
  }
}

// Called when populating session specifics to send to the sync model, called
// when associating models, or updating the sync model.
void SessionModelAssociator::PopulateSessionSpecificsWindow(
    const SessionWindow& window,
    sync_pb::SessionWindow* session_window) {
  DCHECK(CalledOnValidThread());
  session_window->set_window_id(window.window_id.id());
  session_window->set_selected_tab_index(window.selected_tab_index);
  if (window.type == Browser::TYPE_NORMAL) {
    session_window->set_browser_type(
      sync_pb::SessionWindow_BrowserType_TYPE_NORMAL);
  } else if (window.type == Browser::TYPE_POPUP) {
    session_window->set_browser_type(
      sync_pb::SessionWindow_BrowserType_TYPE_POPUP);
  } else {
    // ignore
    LOG(WARNING) << "Session Sync unable to handle windows of type" <<
        window.type;
    return;
  }
  for (std::vector<SessionTab*>::const_iterator i = window.tabs.begin();
      i != window.tabs.end(); ++i) {
    const SessionTab* tab = *i;
    if (!IsValidSessionTab(*tab))
      continue;
    session_window->add_tab(tab->tab_id.id());
  }
}

bool SessionModelAssociator::SyncLocalWindowToSyncModel(
    const SessionWindow& window) {
  DCHECK(CalledOnValidThread());
  DCHECK(tab_map_.empty());
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    SessionTab* tab = window.tabs[i];
    int64 id = tab_pool_.GetFreeTabNode();
    if (id == -1) {
      LOG(ERROR) << "Failed to find/generate free sync node for tab.";
      return false;
    }

    sync_api::WriteTransaction trans(sync_service_->GetUserShare());
    if (!WriteSessionTabToSyncModel(*tab, id, &trans)) {
      return false;
    }

    TabLinks t(id, tab);
    tab_map_[tab->tab_id.id()] = t;
  }
  return true;
}

bool SessionModelAssociator::WriteSessionTabToSyncModel(
    const SessionTab& tab,
    const int64 sync_id,
    sync_api::WriteTransaction* trans) {
  DCHECK(CalledOnValidThread());
  sync_api::WriteNode tab_node(trans);
  if (!tab_node.InitByIdLookup(sync_id)) {
    LOG(ERROR) << "Failed to look up tab node " << sync_id;
    return false;
  }

  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(GetCurrentMachineTag());
  sync_pb::SessionTab* tab_s = specifics.mutable_tab();
  PopulateSessionSpecificsTab(tab, tab_s);
  tab_node.SetSessionSpecifics(specifics);
  return true;
}

// See PopulateSessionSpecificsWindow for use.
void SessionModelAssociator::PopulateSessionSpecificsTab(
    const SessionTab& tab,
    sync_pb::SessionTab* session_tab) {
  DCHECK(CalledOnValidThread());
  session_tab->set_tab_id(tab.tab_id.id());
  session_tab->set_window_id(tab.window_id.id());
  session_tab->set_tab_visual_index(tab.tab_visual_index);
  session_tab->set_current_navigation_index(
      tab.current_navigation_index);
  session_tab->set_pinned(tab.pinned);
  session_tab->set_extension_app_id(tab.extension_app_id);
  for (std::vector<TabNavigation>::const_iterator i =
      tab.navigations.begin(); i != tab.navigations.end(); ++i) {
    const TabNavigation navigation = *i;
    sync_pb::TabNavigation* tab_navigation =
        session_tab->add_navigation();
    PopulateSessionSpecificsNavigation(&navigation, tab_navigation);
  }
}

}  // namespace browser_sync
