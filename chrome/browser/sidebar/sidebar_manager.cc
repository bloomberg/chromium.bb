// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sidebar/sidebar_manager.h"

#include <vector>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_sidebar_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sidebar/sidebar_container.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

struct SidebarManager::SidebarStateForTab {
  // Sidebars linked to this tab.
  ContentIdToSidebarHostMap content_id_to_sidebar_host;
  // Content id of the currently active (expanded and visible) sidebar.
  std::string active_content_id;
};

// static
SidebarManager* SidebarManager::GetInstance() {
  return g_browser_process->sidebar_manager();
}

// static
bool SidebarManager::IsSidebarAllowed() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExperimentalExtensionApis);
}

SidebarManager::SidebarManager() {
}

SidebarContainer* SidebarManager::GetActiveSidebarContainerFor(
    TabContents* tab) {
  TabToSidebarHostMap::iterator it = tab_to_sidebar_host_.find(tab);
  if (it == tab_to_sidebar_host_.end())
    return NULL;
  if (it->second.active_content_id.empty())
    return NULL;
  ContentIdToSidebarHostMap::iterator host_it =
      it->second.content_id_to_sidebar_host.find(it->second.active_content_id);
  DCHECK(host_it != it->second.content_id_to_sidebar_host.end());
  return host_it->second;
}

SidebarContainer* SidebarManager::GetSidebarContainerFor(
    TabContents* tab, const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarHostMap::iterator it = tab_to_sidebar_host_.find(tab);
  if (it == tab_to_sidebar_host_.end())
    return NULL;
  ContentIdToSidebarHostMap::iterator host_it =
      it->second.content_id_to_sidebar_host.find(content_id);
  if (host_it == it->second.content_id_to_sidebar_host.end())
    return NULL;
  return host_it->second;
}

TabContents* SidebarManager::GetSidebarTabContents(
    TabContents* tab, const std::string& content_id) {
  DCHECK(!content_id.empty());
  SidebarContainer* sidebar_host = GetSidebarContainerFor(tab, content_id);
  if (!sidebar_host)
    return NULL;
  return sidebar_host->sidebar_contents();
}

void SidebarManager::NotifyStateChanges(
    TabContents* was_active_sidebar_contents,
    TabContents* active_sidebar_contents) {
  if (was_active_sidebar_contents == active_sidebar_contents)
    return;

  SidebarContainer* was_active_host =
      was_active_sidebar_contents == NULL ? NULL :
          FindSidebarContainerFor(was_active_sidebar_contents);
  SidebarContainer* active_host =
      active_sidebar_contents == NULL ? NULL :
          FindSidebarContainerFor(active_sidebar_contents);

  if (was_active_host != NULL) {
    ExtensionSidebarEventRouter::OnStateChanged(
        was_active_sidebar_contents->profile(),
        was_active_host->tab_contents(), was_active_host->content_id(),
        extension_sidebar_constants::kShownState);
  }

  if (active_host != NULL) {
    ExtensionSidebarEventRouter::OnStateChanged(
        active_sidebar_contents->profile(),
        active_host->tab_contents(), active_host->content_id(),
        extension_sidebar_constants::kActiveState);
  }
}

void SidebarManager::ShowSidebar(TabContents* tab,
                                 const std::string& content_id) {
  DCHECK(!content_id.empty());
  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  if (!host) {
    host = new SidebarContainer(tab, content_id, this);
    RegisterSidebarContainerFor(tab, host);
    // It might trigger UpdateSidebar notification, so load them after
    // the registration.
    host->LoadDefaults();
  }

  host->Show();

  ExtensionSidebarEventRouter::OnStateChanged(
      tab->profile(), tab, content_id,
      extension_sidebar_constants::kShownState);
}

void SidebarManager::ExpandSidebar(TabContents* tab,
                                   const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarHostMap::iterator it = tab_to_sidebar_host_.find(tab);
  if (it == tab_to_sidebar_host_.end())
    return;
  // If it's already active, bail out.
  if (it->second.active_content_id == content_id)
    return;

  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  DCHECK(host);
  if (!host)
    return;
  it->second.active_content_id = content_id;

  host->Expand();
}

void SidebarManager::CollapseSidebar(TabContents* tab,
                                     const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarHostMap::iterator it = tab_to_sidebar_host_.find(tab);
  if (it == tab_to_sidebar_host_.end())
    return;
  // If it's not the one active now, bail out.
  if (it->second.active_content_id != content_id)
    return;

  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  DCHECK(host);
  if (!host)
    return;
  it->second.active_content_id.clear();

  host->Collapse();
}

void SidebarManager::HideSidebar(TabContents* tab,
                                 const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarHostMap::iterator it = tab_to_sidebar_host_.find(tab);
  if (it == tab_to_sidebar_host_.end())
    return;
  if (it->second.active_content_id == content_id)
    it->second.active_content_id.clear();

  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  DCHECK(host);

  UnregisterSidebarContainerFor(tab, content_id);

  ExtensionSidebarEventRouter::OnStateChanged(
      tab->profile(), tab, content_id,
      extension_sidebar_constants::kHiddenState);
}

void SidebarManager::NavigateSidebar(TabContents* tab,
                                     const std::string& content_id,
                                     const GURL& url) {
  DCHECK(!content_id.empty());
  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  if (!host)
    return;

  host->Navigate(url);
}

void SidebarManager::SetSidebarBadgeText(
    TabContents* tab, const std::string& content_id,
    const string16& badge_text) {
  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  if (!host)
    return;
  host->SetBadgeText(badge_text);
}

void SidebarManager::SetSidebarIcon(
    TabContents* tab, const std::string& content_id,
    const SkBitmap& bitmap) {
  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  if (!host)
    return;
  host->SetIcon(bitmap);
}

void SidebarManager::SetSidebarTitle(
    TabContents* tab, const std::string& content_id,
    const string16& title) {
  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  if (!host)
    return;
  host->SetTitle(title);
}

SidebarManager::~SidebarManager() {
  DCHECK(tab_to_sidebar_host_.empty());
  DCHECK(sidebar_host_to_tab_.empty());
}

void SidebarManager::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::TAB_CONTENTS_DESTROYED) {
    HideAllSidebars(Source<TabContents>(source).ptr());
  } else {
    NOTREACHED() << "Got a notification we didn't register for!";
  }
}

void SidebarManager::UpdateSidebar(SidebarContainer* host) {
  NotificationService::current()->Notify(
      NotificationType::SIDEBAR_CHANGED,
      Source<SidebarManager>(this),
      Details<SidebarContainer>(host));
}

void SidebarManager::HideAllSidebars(TabContents* tab) {
  TabToSidebarHostMap::iterator tab_it = tab_to_sidebar_host_.find(tab);
  if (tab_it == tab_to_sidebar_host_.end())
    return;
  const ContentIdToSidebarHostMap& hosts =
      tab_it->second.content_id_to_sidebar_host;

  std::vector<std::string> content_ids;
  for (ContentIdToSidebarHostMap::const_iterator it = hosts.begin();
       it != hosts.end(); ++it) {
    content_ids.push_back(it->first);
  }

  for (std::vector<std::string>::iterator it = content_ids.begin();
       it != content_ids.end(); ++it) {
    HideSidebar(tab, *it);
  }
}

SidebarContainer* SidebarManager::FindSidebarContainerFor(
    TabContents* sidebar_contents) {
  for (SidebarHostToTabMap::iterator it = sidebar_host_to_tab_.begin();
       it != sidebar_host_to_tab_.end();
       ++it) {
    if (sidebar_contents == it->first->sidebar_contents())
      return it->first;
  }
  return NULL;
}

void SidebarManager::RegisterSidebarContainerFor(
    TabContents* tab, SidebarContainer* sidebar_host) {
  DCHECK(!GetSidebarContainerFor(tab, sidebar_host->content_id()));

  // If it's a first sidebar for this tab, register destroy notification.
  if (tab_to_sidebar_host_.find(tab) == tab_to_sidebar_host_.end()) {
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab));
  }

  BindSidebarHost(tab, sidebar_host);
}

void SidebarManager::UnregisterSidebarContainerFor(
      TabContents* tab, const std::string& content_id) {
  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  DCHECK(host);
  if (!host)
    return;

  UnbindSidebarHost(tab, host);

  // If there's no more sidebars linked to this tab, unsubscribe.
  if (tab_to_sidebar_host_.find(tab) == tab_to_sidebar_host_.end()) {
    registrar_.Remove(this,
                      NotificationType::TAB_CONTENTS_DESTROYED,
                      Source<TabContents>(tab));
  }

  // Issue tab closing event post unbound.
  host->SidebarClosing();
  // Destroy sidebar container.
  delete host;
}

void SidebarManager::BindSidebarHost(TabContents* tab,
                                     SidebarContainer* sidebar_host) {
  const std::string& content_id = sidebar_host->content_id();

  DCHECK(GetSidebarContainerFor(tab, content_id) == NULL);
  DCHECK(sidebar_host_to_tab_.find(sidebar_host) ==
         sidebar_host_to_tab_.end());

  tab_to_sidebar_host_[tab].content_id_to_sidebar_host[content_id] =
      sidebar_host;
  sidebar_host_to_tab_[sidebar_host] = tab;
}

void SidebarManager::UnbindSidebarHost(TabContents* tab,
                                       SidebarContainer* sidebar_host) {
  const std::string& content_id = sidebar_host->content_id();

  DCHECK(GetSidebarContainerFor(tab, content_id) == sidebar_host);
  DCHECK(sidebar_host_to_tab_.find(sidebar_host)->second == tab);
  DCHECK(tab_to_sidebar_host_[tab].active_content_id != content_id);

  tab_to_sidebar_host_[tab].content_id_to_sidebar_host.erase(content_id);
  if (tab_to_sidebar_host_[tab].content_id_to_sidebar_host.empty())
    tab_to_sidebar_host_.erase(tab);
  sidebar_host_to_tab_.erase(sidebar_host);
}
