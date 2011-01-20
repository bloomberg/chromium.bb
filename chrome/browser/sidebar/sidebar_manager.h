// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_H_
#define CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_H_

#include <map>
#include <string>

#include "base/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/sidebar/sidebar_container.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class GURL;
class PrefService;
class Profile;
class SidebarContainer;
class SkBitmap;
class TabContents;

///////////////////////////////////////////////////////////////////////////////
// SidebarManager
//
//  This class is a singleton that manages SidebarContainer instances and
//  maintains a connection between tabs and sidebars.
//
class SidebarManager : public NotificationObserver,
                       public base::RefCounted<SidebarManager>,
                       private SidebarContainer::Delegate {
 public:
  // Returns s singleton instance.
  static SidebarManager* GetInstance();

  // Returns true if sidebar is allowed to be displayed in the browser.
  static bool IsSidebarAllowed();

  SidebarManager();

  // Returns SidebarContainer registered for |tab| and active or NULL if
  // there is no alive and active SidebarContainer registered for |tab|.
  SidebarContainer* GetActiveSidebarContainerFor(TabContents* tab);

  // Returns SidebarContainer registered for |tab| and |content_id| or NULL if
  // there is no such SidebarContainer registered.
  SidebarContainer* GetSidebarContainerFor(TabContents* tab,
                                           const std::string& content_id);

  // Returns sidebar's TabContents registered for |tab| and |content_id|.
  TabContents* GetSidebarTabContents(TabContents* tab,
                                     const std::string& content_id);

  // Sends sidebar state change notification to extensions.
  void NotifyStateChanges(TabContents* was_active_sidebar_contents,
                          TabContents* active_sidebar_contents);

  // Functions supporting chrome.experimental.sidebar API.

  // Shows sidebar identified by |tab| and |content_id| (only sidebar's
  // mini tab is visible).
  void ShowSidebar(TabContents* tab, const std::string& content_id);

  // Expands sidebar identified by |tab| and |content_id|.
  void ExpandSidebar(TabContents* tab, const std::string& content_id);

  // Collapses sidebar identified by |tab| and |content_id| (has no effect
  // if sidebar is not expanded).
  void CollapseSidebar(TabContents* tab, const std::string& content_id);

  // Hides sidebar identified by |tab| and |content_id| (removes sidebar's
  // mini tab).
  void HideSidebar(TabContents* tab, const std::string& content_id);

  // Navigates sidebar identified by |tab| and |content_id| to |url|.
  void NavigateSidebar(TabContents* tab,
                       const std::string& content_id,
                       const GURL& url);

  // Changes sidebar's badge text (displayed on the mini tab).
  void SetSidebarBadgeText(TabContents* tab,
                           const std::string& content_id,
                           const string16& badge_text);

  // Changes sidebar's icon (displayed on the mini tab).
  void SetSidebarIcon(TabContents* tab,
                      const std::string& content_id,
                      const SkBitmap& bitmap);

  // Changes sidebar's title (mini tab's tooltip).
  void SetSidebarTitle(TabContents* tab,
                       const std::string& content_id,
                       const string16& title);

 private:
  friend class base::RefCounted<SidebarManager>;

  virtual ~SidebarManager();

  // Overridden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from SidebarContainer::Delegate.
  virtual void UpdateSidebar(SidebarContainer* host);

  // Hides all sidebars registered for |tab|.
  void HideAllSidebars(TabContents* tab);

  // Returns SidebarContainer corresponding to |sidebar_contents|.
  SidebarContainer* FindSidebarContainerFor(TabContents* sidebar_contents);

  // Registers new SidebarContainer for |tab|. There must be no
  // other SidebarContainers registered for the RenderViewHost at the moment.
  void RegisterSidebarContainerFor(TabContents* tab,
                                   SidebarContainer* container);

  // Unregisters SidebarContainer identified by |tab| and |content_id|.
  void UnregisterSidebarContainerFor(TabContents* tab,
                                     const std::string& content_id);

  // Records the link between |tab| and |sidebar_host|.
  void BindSidebarHost(TabContents* tab, SidebarContainer* sidebar_host);

  // Forgets the link between |tab| and |sidebar_host|.
  void UnbindSidebarHost(TabContents* tab, SidebarContainer* sidebar_host);

  NotificationRegistrar registrar_;

  // This map stores sidebars linked to a particular tab. Sidebars are
  // identified by their unique content id (string).
  typedef std::map<std::string, SidebarContainer*> ContentIdToSidebarHostMap;

  // These two maps are for tracking dependencies between tabs and
  // their SidebarContainers.
  //
  // SidebarManager start listening to SidebarContainers when they are put
  // into these maps and removes them when they are closing.
  struct SidebarStateForTab;
  typedef std::map<TabContents*, SidebarStateForTab> TabToSidebarHostMap;
  TabToSidebarHostMap tab_to_sidebar_host_;

  typedef std::map<SidebarContainer*, TabContents*> SidebarHostToTabMap;
  SidebarHostToTabMap sidebar_host_to_tab_;

  DISALLOW_COPY_AND_ASSIGN(SidebarManager);
};

#endif  // CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_H_

