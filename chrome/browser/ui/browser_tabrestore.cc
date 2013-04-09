// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tabrestore.h"

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

using content::WebContents;
using content::NavigationController;
using content::NavigationEntry;

namespace chrome {

namespace {

NavigationController::RestoreType GetRestoreType(Browser* browser,
                                                 bool from_last_session) {
  if (!from_last_session)
    return NavigationController::RESTORE_CURRENT_SESSION;
  return browser->profile()->GetLastSessionExitType() == Profile::EXIT_CRASHED ?
      NavigationController::RESTORE_LAST_SESSION_CRASHED :
      NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY;
}

WebContents* CreateRestoredTab(
    Browser* browser,
    const std::vector<TabNavigation>& navigations,
    int selected_navigation,
    const std::string& extension_app_id,
    bool from_last_session,
    content::SessionStorageNamespace* session_storage_namespace,
    const std::string& user_agent_override) {
  GURL restore_url = navigations.at(selected_navigation).virtual_url();
  // TODO(ajwong): Remove the temporary session_storage_namespace_map when
  // we teach session restore to understand that one tab can have multiple
  // SessionStorageNamespace objects. Also remove the
  // session_storage_namespace.h include since we only need that to assign
  // into the map.
  content::SessionStorageNamespaceMap session_storage_namespace_map;
  session_storage_namespace_map[""] = session_storage_namespace;
  WebContents::CreateParams create_params(
      browser->profile(),
      tab_util::GetSiteInstanceForNewTab(browser->profile(), restore_url));
  WebContents* base_web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (base_web_contents) {
    create_params.initial_size =
        base_web_contents->GetView()->GetContainerSize();
  }
  WebContents* web_contents = content::WebContents::CreateWithSessionStorage(
      create_params,
      session_storage_namespace_map);
  extensions::TabHelper::CreateForWebContents(web_contents);
  extensions::TabHelper::FromWebContents(web_contents)->
      SetExtensionAppById(extension_app_id);
  std::vector<NavigationEntry*> entries =
      TabNavigation::CreateNavigationEntriesFromTabNavigations(
          navigations, browser->profile());
  web_contents->SetUserAgentOverride(user_agent_override);
  web_contents->GetController().Restore(
      selected_navigation, GetRestoreType(browser, from_last_session),
      &entries);
  DCHECK_EQ(0u, entries.size());

  return web_contents;
}

}  // namespace

content::WebContents* AddRestoredTab(
    Browser* browser,
    const std::vector<TabNavigation>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    bool select,
    bool pin,
    bool from_last_session,
    content::SessionStorageNamespace* session_storage_namespace,
    const std::string& user_agent_override) {
  WebContents* web_contents = CreateRestoredTab(browser,
                                                navigations,
                                                selected_navigation,
                                                extension_app_id,
                                                from_last_session,
                                                session_storage_namespace,
                                                user_agent_override);

  int add_types = select ? TabStripModel::ADD_ACTIVE
                         : TabStripModel::ADD_NONE;
  if (pin) {
    int first_mini_tab_idx =
        browser->tab_strip_model()->IndexOfFirstNonMiniTab();
    tab_index = std::min(tab_index, first_mini_tab_idx);
    add_types |= TabStripModel::ADD_PINNED;
  }
  browser->tab_strip_model()->InsertWebContentsAt(tab_index, web_contents,
                                                  add_types);
  if (select) {
    browser->window()->Activate();
  } else {
    // We set the size of the view here, before WebKit does its initial
    // layout.  If we don't, the initial layout of background tabs will be
    // performed with a view width of 0, which may cause script outputs and
    // anchor link location calculations to be incorrect even after a new
    // layout with proper view dimensions. TabStripModel::AddWebContents()
    // contains similar logic.
    web_contents->GetView()->SizeContents(
        browser->window()->GetRestoredBounds().size());
    web_contents->WasHidden();
  }
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (session_service)
    session_service->TabRestored(web_contents, pin);
  return web_contents;
}

void ReplaceRestoredTab(
    Browser* browser,
    const std::vector<TabNavigation>& navigations,
    int selected_navigation,
    bool from_last_session,
    const std::string& extension_app_id,
    content::SessionStorageNamespace* session_storage_namespace,
    const std::string& user_agent_override) {
  WebContents* web_contents = CreateRestoredTab(browser,
                                                navigations,
                                                selected_navigation,
                                                extension_app_id,
                                                from_last_session,
                                                session_storage_namespace,
                                                user_agent_override);

  // ReplaceWebContentsAt won't animate in the restoration, so manually do the
  // equivalent of ReplaceWebContentsAt.
  TabStripModel* tab_strip = browser->tab_strip_model();
  int insertion_index = tab_strip->active_index();
  tab_strip->InsertWebContentsAt(insertion_index + 1,
                                 web_contents,
                                 TabStripModel::ADD_ACTIVE |
                                 TabStripModel::ADD_INHERIT_GROUP);
  tab_strip->CloseWebContentsAt(insertion_index, TabStripModel::CLOSE_NONE);
}

}  // namespace chrome
