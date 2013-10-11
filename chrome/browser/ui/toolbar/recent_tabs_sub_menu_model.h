// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_SUB_MENU_MODEL_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
struct SessionTab;

namespace browser_sync {
class SessionModelAssociator;
}

namespace chrome {
struct FaviconImageResult;
}

namespace gfx {
class Image;
}

namespace ui {
class AcceleratorProvider;
}

// A menu model that builds the contents of "Recent tabs" submenu, which include
// the recently closed tabs/windows of current device i.e. local entries, and
// opened tabs of other devices.
class RecentTabsSubMenuModel : public ui::SimpleMenuModel,
                               public ui::SimpleMenuModel::Delegate,
                               public TabRestoreServiceObserver {
 public:
  // Command Id for recently closed items header or disabled item to which the
  // accelerator string will be appended.
  static const int kRecentlyClosedHeaderCommandId;
  static const int kDisabledRecentlyClosedHeaderCommandId;

  // If |associator| is NULL, default associator for |browser|'s profile will
  // be used.  Testing may require a specific |associator|.
  RecentTabsSubMenuModel(ui::AcceleratorProvider* accelerator_provider,
                         Browser* browser,
                         browser_sync::SessionModelAssociator* associator);
  virtual ~RecentTabsSubMenuModel();

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual const gfx::Font* GetLabelFontAt(int index) const OVERRIDE;

  int GetMaxWidthForItemAtIndex(int item_index) const;
  bool GetURLAndTitleForItemAtIndex(int index,
                                    std::string* url,
                                    string16* title);

 private:
  struct TabNavigationItem;
  typedef std::vector<TabNavigationItem> TabNavigationItems;

  typedef std::vector<SessionID::id_type> WindowItems;

  // Build the menu items by populating the menumodel.
  void Build();

  // Build the recently closed tabs and windows items.
  void BuildLocalEntries();

  // Build the tabs items from other devices.
  void BuildTabsFromOtherDevices();

  // Build a recently closed tab item with parameters needed to restore it, and
  // add it to the menumodel at |curr_model_index|.
  void BuildLocalTabItem(int seesion_id,
                         const string16& title,
                         const GURL& url,
                         int curr_model_index);

  // Build the recently closed window item with parameters needed to restore it,
  // and add it to the menumodel at |curr_model_index|.
  void BuildLocalWindowItem(const SessionID::id_type& window_id,
                            int num_tabs,
                            int curr_model_index);

  // Build the tab item for other devices with parameters needed to restore it.
  void BuildOtherDevicesTabItem(const std::string& session_tag,
                                const SessionTab& tab);

  // Add the favicon for the device section header.
  void AddDeviceFavicon(int index_in_menu,
                        browser_sync::SyncedSession::DeviceType device_type);

  // Add the favicon for a local or other devices' tab asynchronously,
  // OnFaviconDataAvailable() will be invoked when the favicon is ready.
  void AddTabFavicon(int command_id, const GURL& url);
  void OnFaviconDataAvailable(int command_id,
                              const chrome::FaviconImageResult& image_result);

  // Clear all recently closed tabs and windows.
  void ClearLocalEntries();

  // Converts |command_id| of menu item to index in local or other devices'
  // TabNavigationItems, and returns the corresponding local or other devices'
  // TabNavigationItems in |tab_items|.
  int CommandIdToTabVectorIndex(int command_id, TabNavigationItems** tab_items);
  browser_sync::SessionModelAssociator* GetModelAssociator();

  // Overridden from TabRestoreServiceObserver:
  virtual void TabRestoreServiceChanged(TabRestoreService* service) OVERRIDE;
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service) OVERRIDE;

  Browser* browser_;  // Weak.

  browser_sync::SessionModelAssociator* associator_;  // Weak.

  // Accelerator for reopening last closed tab.
  ui::Accelerator reopen_closed_tab_accelerator_;

  // Navigation items for local recently closed tabs.  The |command_id| for
  // these is set to |kFirstLocalTabCommandId| plus the index into the vector.
  // Upon invocation of the menu, the navigation information is retrieved from
  // |local_tab_navigation_items_| and used to navigate to the item specified.
  TabNavigationItems local_tab_navigation_items_;

  // Similar to |local_tab_navigation_items_| except the tabs are opened tabs
  // from other devices, and the first |command_id| is
  // |kFirstOtherDevicesTabCommandId|.
  TabNavigationItems other_devices_tab_navigation_items_;

  // Window items for local recently closed windows.  The |command_id| for
  // these is set to |kFirstLocalWindowCommandId| plus the index into the
  // vector.  Upon invocation of the menu, information is retrieved from
  // |local_window_items_| and used to create the specified window.
  WindowItems local_window_items_;

  // Index of the last local entry (recently closed tab or window) in the
  // menumodel.
  int last_local_model_index_;

  gfx::Image default_favicon_;

  CancelableTaskTracker local_tab_cancelable_task_tracker_;
  CancelableTaskTracker other_devices_tab_cancelable_task_tracker_;

  base::WeakPtrFactory<RecentTabsSubMenuModel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabsSubMenuModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_SUB_MENU_MODEL_H_
