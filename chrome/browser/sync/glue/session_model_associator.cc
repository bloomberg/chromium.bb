// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/session_model_associator.h"

#include <utility>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"

namespace browser_sync {

namespace {

static const char kNoSessionsFolderError[] =
    "Server did not create the top-level sessions node. We "
    "might be running against an out-of-date server.";

}  // namespace

SessionModelAssociator::SessionModelAssociator(
    ProfileSyncService* sync_service) : sync_service_(sync_service) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_service_);
}

SessionModelAssociator::~SessionModelAssociator() {
  DCHECK(CalledOnValidThread());
}

// Sends a notification to ForeignSessionHandler to update the UI, because
// the session corresponding to the id given has changed state.
void SessionModelAssociator::Associate(
    const sync_pb::SessionSpecifics* specifics, int64 sync_id) {
  DCHECK(CalledOnValidThread());
  NotificationService::current()->Notify(
      NotificationType::FOREIGN_SESSION_UPDATED,
      NotificationService::AllSources(),
      Details<int64>(&sync_id));
}

bool SessionModelAssociator::AssociateModels() {
  DCHECK(CalledOnValidThread());
  UpdateSyncModelDataFromClient();
  return true;
}

bool SessionModelAssociator::ChromeModelHasUserCreatedNodes(
    bool* has_nodes) {
  DCHECK(CalledOnValidThread());
  CHECK(has_nodes);
  // This is wrong, but this function is unused, anyway.
  *has_nodes = true;
  return true;
}

// Sends a notification to ForeignSessionHandler to update the UI, because
// the session corresponding to the id given has been deleted.
void SessionModelAssociator::Disassociate(int64 sync_id) {
  NotificationService::current()->Notify(
      NotificationType::FOREIGN_SESSION_DELETED,
      NotificationService::AllSources(),
      Details<int64>(&sync_id));
}

const sync_pb::SessionSpecifics* SessionModelAssociator::
    GetChromeNodeFromSyncId(int64 sync_id) {
  sync_api::ReadTransaction trans(
    sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode node(&trans);
  if (!node.InitByIdLookup(sync_id))
    return NULL;
  return new sync_pb::SessionSpecifics(node.GetSessionSpecifics());
}

bool SessionModelAssociator::GetSyncIdForTaggedNode(const std::string* tag,
    int64* sync_id) {
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode node(&trans);
  if (!node.InitByClientTagLookup(syncable::SESSIONS, *tag))
    return false;
  *sync_id = node.GetId();
  return true;
}

int64 SessionModelAssociator::GetSyncIdFromChromeId(std::string id) {
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode node(&trans);
  if (!node.InitByClientTagLookup(syncable::SESSIONS, id))
    return sync_api::kInvalidId;
  return node.GetId();
}

bool SessionModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(CalledOnValidThread());
  CHECK(has_nodes);
  *has_nodes = false;
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
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

std::string SessionModelAssociator::GetCurrentMachineTag() {
  if (current_machine_tag_.empty())
    InitializeCurrentMachineTag();
  DCHECK(!current_machine_tag_.empty());
  return current_machine_tag_;
}

void SessionModelAssociator::AppendForeignSessionFromSpecifics(
    const sync_pb::SessionSpecifics* specifics,
    std::vector<ForeignSession*>* session) {
  ForeignSession* foreign_session = new ForeignSession();
  foreign_session->foreign_tession_tag = specifics->session_tag();
  session->insert(session->end(), foreign_session);
  for (int i = 0; i < specifics->session_window_size(); i++) {
    const sync_pb::SessionWindow* window = &specifics->session_window(i);
    SessionWindow* session_window = new SessionWindow();
    PopulateSessionWindowFromSpecifics(session_window, window);
    foreign_session->windows.insert(
        foreign_session->windows.end(), session_window);
  }
}

// Fills the given vector with foreign session windows to restore.
void SessionModelAssociator::AppendForeignSessionWithID(int64 id,
    std::vector<ForeignSession*>* session, sync_api::BaseTransaction* trans) {
  if (id == sync_api::kInvalidId)
    return;
  sync_api::ReadNode node(trans);
  if (!node.InitByIdLookup(id))
    return;
  const sync_pb::SessionSpecifics* ref = &node.GetSessionSpecifics();
  AppendForeignSessionFromSpecifics(ref, session);
}

void SessionModelAssociator::UpdateSyncModelDataFromClient() {
  DCHECK(CalledOnValidThread());
  SessionService::SessionCallback* callback =
      NewCallback(this, &SessionModelAssociator::OnGotSession);
  // TODO(jerrica): Stop current race condition, possibly make new method in
  // session service, which only grabs the windows from memory.
  GetSessionService()->GetCurrentSession(&consumer_, callback);
}

bool SessionModelAssociator::GetSessionDataFromSyncModel(
    std::vector<ForeignSession*>* sessions) {
  std::vector<const sync_pb::SessionSpecifics*> specifics;
  DCHECK(CalledOnValidThread());
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(kSessionsTag)) {
    LOG(ERROR) << kNoSessionsFolderError;
    return false;
  }
  sync_api::ReadNode current_machine(&trans);
  int64 current_id = (current_machine.InitByClientTagLookup(syncable::SESSIONS,
    GetCurrentMachineTag())) ? current_machine.GetId() : sync_api::kInvalidId;
  // Iterate through the nodes and populate the session model.
  int64 id = root.GetFirstChildId();
  while (id != sync_api::kInvalidId) {
    sync_api::ReadNode sync_node(&trans);
    if (!sync_node.InitByIdLookup(id)) {
      LOG(ERROR) << "Failed to fetch sync node for id " << id;
      return false;
    }
    if (id != current_id) {
      specifics.insert(specifics.end(), &sync_node.GetSessionSpecifics());
    }
    id = sync_node.GetSuccessorId();
  }
  for (std::vector<const sync_pb::SessionSpecifics*>::const_iterator i =
    specifics.begin(); i != specifics.end(); ++i) {
    AppendForeignSessionFromSpecifics(*i, sessions);
  }
  return true;
}

SessionService* SessionModelAssociator::GetSessionService() {
  DCHECK(sync_service_);
  Profile* profile = sync_service_->profile();
  DCHECK(profile);
  SessionService* sessions_service = profile->GetSessionService();
  DCHECK(sessions_service);
  return sessions_service;
}

void SessionModelAssociator::InitializeCurrentMachineTag() {
  sync_api::WriteTransaction trans(sync_service_->backend()->
      GetUserShareHandle());
  syncable::Directory* dir =
      trans.GetWrappedWriteTrans()->directory();
  current_machine_tag_ = "session_sync";
  current_machine_tag_.append(dir->cache_guid());
}

// See PopulateSessionSpecificsTab for use.  May add functionality that includes
// the state later.
void SessionModelAssociator::PopulateSessionSpecificsNavigation(
  const TabNavigation* navigation, sync_pb::TabNavigation* tab_navigation) {
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

// See PopulateSessionSpecificsWindow for use.
void SessionModelAssociator::PopulateSessionSpecificsTab(
  const SessionTab* tab, sync_pb::SessionTab* session_tab) {
  session_tab->set_tab_visual_index(tab->tab_visual_index);
  session_tab->set_current_navigation_index(
      tab->current_navigation_index);
  session_tab->set_pinned(tab->pinned);
  session_tab->set_extension_app_id(tab->extension_app_id);
  for (std::vector<TabNavigation>::const_iterator i3 =
      tab->navigations.begin(); i3 != tab->navigations.end(); ++i3) {
    const TabNavigation navigation = *i3;
    sync_pb::TabNavigation* tab_navigation =
        session_tab->add_navigation();
    PopulateSessionSpecificsNavigation(&navigation, tab_navigation);
  }
}

// Called when populating session specifics to send to the sync model, called
// when associating models, or updating the sync model.
void SessionModelAssociator::PopulateSessionSpecificsWindow(
  const SessionWindow* window, sync_pb::SessionWindow* session_window) {
    session_window->set_selected_tab_index(window->selected_tab_index);
    if (window->type == 1) {
      session_window->set_browser_type(
        sync_pb::SessionWindow_BrowserType_TYPE_NORMAL);
    } else {
      session_window->set_browser_type(
        sync_pb::SessionWindow_BrowserType_TYPE_POPUP);
    }
    for (std::vector<SessionTab*>::const_iterator i2 = window->tabs.begin();
        i2 != window->tabs.end(); ++i2) {
      const SessionTab* tab = *i2;
      if (tab->navigations.at(tab->current_navigation_index).virtual_url() ==
        GURL(chrome::kChromeUINewTabURL)) {
        continue;
      }
      sync_pb::SessionTab* session_tab = session_window->add_session_tab();
      PopulateSessionSpecificsTab(tab, session_tab);
    }
}

bool SessionModelAssociator::WindowHasNoTabsToSync(
    const SessionWindow* window) {
  int num_populated = 0;
  for (std::vector<SessionTab*>::const_iterator i = window->tabs.begin();
      i != window->tabs.end(); ++i) {
    const SessionTab* tab = *i;
    if (tab->navigations.at(tab->current_navigation_index).virtual_url() ==
        GURL(chrome::kChromeUINewTabURL)) {
      continue;
    }
    num_populated++;
  }
  if (num_populated == 0)
    return true;
  return false;
}

void SessionModelAssociator::OnGotSession(int handle,
    std::vector<SessionWindow*>* windows) {
  sync_pb::SessionSpecifics session;
  // Set the tag, then iterate through the vector of windows, extracting the
  // window data, along with the tabs data and tab navigation data to populate
  // the session specifics.
  session.set_session_tag(GetCurrentMachineTag());
  for (std::vector<SessionWindow*>::const_iterator i = windows->begin();
      i != windows->end(); ++i) {
    const SessionWindow* window = *i;
    if (WindowHasNoTabsToSync(window)) {
       continue;
    }
    sync_pb::SessionWindow* session_window = session.add_session_window();
    PopulateSessionSpecificsWindow(window, session_window);
  }
  bool has_nodes = false;
  if (!SyncModelHasUserCreatedNodes(&has_nodes))
    return;
  if (session.session_window_size() == 0 && has_nodes)
    return;
  sync_api::WriteTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(kSessionsTag)) {
    LOG(ERROR) << kNoSessionsFolderError;
    return;
  }
  UpdateSyncModel(&session, &trans, &root);
}

void SessionModelAssociator::AppendSessionTabNavigation(
    std::vector<TabNavigation>* navigations,
    const sync_pb::TabNavigation* navigation) {
  int index = 0;
  GURL virtual_url;
  GURL referrer;
  string16 title;
  std::string state;
  PageTransition::Type transition(PageTransition::LINK);
  if (navigation->has_index())
    index = navigation->index();
  if (navigation->has_virtual_url()) {
    GURL gurl(navigation->virtual_url());
    virtual_url = gurl;
  }
  if (navigation->has_referrer()) {
    GURL gurl(navigation->referrer());
    referrer = gurl;
  }
  if (navigation->has_title())
    title = UTF8ToUTF16(navigation->title());
  if (navigation->has_page_transition() ||
      navigation->has_navigation_qualifier()) {
    switch (navigation->page_transition()) {
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
        switch (navigation->navigation_qualifier()) {
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

void SessionModelAssociator::PopulateSessionTabFromSpecifics(
    SessionTab* session_tab,
    const sync_pb::SessionTab* tab, SessionID id) {
  session_tab->window_id = id;
  SessionID tabID;
  session_tab->tab_id = tabID;
  if (tab->has_tab_visual_index())
    session_tab->tab_visual_index = tab->tab_visual_index();
  if (tab->has_current_navigation_index()) {
    session_tab->current_navigation_index =
    tab->current_navigation_index();
  }
  if (tab->has_pinned())
    session_tab->pinned = tab->pinned();
  if (tab->has_extension_app_id())
    session_tab->extension_app_id = tab->extension_app_id();
  for (int i3 = 0; i3 < tab->navigation_size(); i3++) {
    const sync_pb::TabNavigation* navigation = &tab->navigation(i3);
    AppendSessionTabNavigation(&session_tab->navigations, navigation);
  }
}

void SessionModelAssociator::PopulateSessionWindowFromSpecifics(
    SessionWindow* session_window, const sync_pb::SessionWindow* window) {
  SessionID id;
  session_window->window_id = id;
  if (window->has_selected_tab_index())
    session_window->selected_tab_index = window->selected_tab_index();
  if (window->has_browser_type()) {
    if (window->browser_type() ==
        sync_pb::SessionWindow_BrowserType_TYPE_NORMAL) {
      session_window->type = 1;
    } else {
      session_window->type = 2;
    }
  }
  for (int i = 0; i < window->session_tab_size(); i++) {
    const sync_pb::SessionTab& tab = window->session_tab(i);
    SessionTab* session_tab = new SessionTab();
    PopulateSessionTabFromSpecifics(session_tab, &tab, id);
    session_window->tabs.insert(session_window->tabs.end(), session_tab);
  }
}

bool SessionModelAssociator::UpdateSyncModel(
    sync_pb::SessionSpecifics* session_data,
    sync_api::WriteTransaction* trans,
    const sync_api::ReadNode* root) {
  const std::string id = session_data->session_tag();
  sync_api::WriteNode write_node(trans);
  if (!write_node.InitByClientTagLookup(syncable::SESSIONS, id)) {
    sync_api::WriteNode create_node(trans);
    if (!create_node.InitUniqueByCreation(syncable::SESSIONS, *root, id)) {
      LOG(ERROR) << "Could not create node for session " << id;
      return false;
    }
    create_node.SetSessionSpecifics(*session_data);
    return true;
  }
  write_node.SetSessionSpecifics(*session_data);
  return true;
}

}  // namespace browser_sync

