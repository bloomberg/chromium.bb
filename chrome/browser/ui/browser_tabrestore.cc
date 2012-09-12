// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tabrestore.h"

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ipc/ipc_message.h"

using content::WebContents;
using content::NavigationController;
using content::NavigationEntry;

namespace chrome {

int GetIndexForInsertionDuringRestore(Browser* browser, int relative_index) {
  return (browser->tab_strip_model()->insertion_policy() ==
      TabStripModel::INSERT_AFTER) ? browser->tab_count() : relative_index;
}

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
  GURL restore_url = navigations.at(selected_navigation).virtual_url();
  // TODO(ajwong): Remove the temporary session_storage_namespace_map when
  // we teach session restore to understand that one tab can have multiple
  // SessionStorageNamespace objects. Also remove the
  // session_storage_namespace.h include since we only need that to assign
  // into the map.
  content::SessionStorageNamespaceMap session_storage_namespace_map;
  session_storage_namespace_map[""] = session_storage_namespace;
  TabContents* tab_contents = chrome::TabContentsWithSessionStorageFactory(
      browser->profile(),
      tab_util::GetSiteInstanceForNewTab(browser->profile(), restore_url),
      MSG_ROUTING_NONE,
      chrome::GetActiveWebContents(browser),
      session_storage_namespace_map);
  WebContents* new_tab = tab_contents->web_contents();
  extensions::TabHelper::FromWebContents(new_tab)->
      SetExtensionAppById(extension_app_id);
  std::vector<NavigationEntry*> entries;
  TabNavigation::CreateNavigationEntriesFromTabNavigations(
      browser->profile(), navigations, &entries);
  new_tab->SetUserAgentOverride(user_agent_override);
  new_tab->GetController().Restore(
      selected_navigation, from_last_session, &entries);
  DCHECK_EQ(0u, entries.size());

  int add_types = select ? TabStripModel::ADD_ACTIVE :
      TabStripModel::ADD_NONE;
  if (pin) {
    int first_mini_tab_idx =
        browser->tab_strip_model()->IndexOfFirstNonMiniTab();
    tab_index = std::min(tab_index, first_mini_tab_idx);
    add_types |= TabStripModel::ADD_PINNED;
  }
  browser->tab_strip_model()->InsertTabContentsAt(tab_index, tab_contents,
                                                  add_types);
  if (select) {
    browser->window()->Activate();
  } else {
    // We set the size of the view here, before WebKit does its initial
    // layout.  If we don't, the initial layout of background tabs will be
    // performed with a view width of 0, which may cause script outputs and
    // anchor link location calculations to be incorrect even after a new
    // layout with proper view dimensions. TabStripModel::AddTabContents()
    // contains similar logic.
    new_tab->GetView()->SizeContents(
        browser->window()->GetRestoredBounds().size());
    new_tab->WasHidden();
  }
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (session_service)
    session_service->TabRestored(tab_contents, pin);
  return new_tab;
}

void ReplaceRestoredTab(
    Browser* browser,
    const std::vector<TabNavigation>& navigations,
    int selected_navigation,
    bool from_last_session,
    const std::string& extension_app_id,
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
  TabContents* tab_contents = chrome::TabContentsWithSessionStorageFactory(
      browser->profile(),
      tab_util::GetSiteInstanceForNewTab(browser->profile(), restore_url),
      MSG_ROUTING_NONE,
      GetActiveWebContents(browser),
      session_storage_namespace_map);
  WebContents* replacement = tab_contents->web_contents();
  extensions::TabHelper::FromWebContents(replacement)->
      SetExtensionAppById(extension_app_id);
  replacement->SetUserAgentOverride(user_agent_override);
  std::vector<NavigationEntry*> entries;
  TabNavigation::CreateNavigationEntriesFromTabNavigations(
      browser->profile(), navigations, &entries);
  replacement->GetController().Restore(
      selected_navigation, from_last_session, &entries);
  DCHECK_EQ(0u, entries.size());

  browser->tab_strip_model()->ReplaceNavigationControllerAt(
      browser->active_index(),
      tab_contents);
}

}  // namespace chrome
