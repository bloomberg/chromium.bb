// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_SUB_MENU_MODEL_H_

#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/elapsed_timer.h"
#include "components/favicon/core/favicon_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "components/sync_sessions/synced_session.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

namespace favicon_base {
struct FaviconImageResult;
}

namespace gfx {
class Image;
}

namespace sessions {
struct SessionTab;
}

namespace sync_sessions {
class OpenTabsUIDelegate;
}

namespace ui {
class AcceleratorProvider;
}

// A menu model that builds the contents of "Recent tabs" submenu, which include
// the recently closed tabs/windows of current device i.e. local entries, and
// opened tabs of other devices.
class RecentTabsSubMenuModel : public ui::SimpleMenuModel,
                               public ui::SimpleMenuModel::Delegate,
                               public sessions::TabRestoreServiceObserver {
 public:
  // Command Id for recently closed items header or disabled item to which the
  // accelerator string will be appended.
  static const int kRecentlyClosedHeaderCommandId;
  static const int kDisabledRecentlyClosedHeaderCommandId;

  // Exposed for tests only: return the Command Id for the first entry in the
  // recently closed window items list.
  static int GetFirstRecentTabsCommandId();

  // If |open_tabs_delegate| is NULL, the default delegate for |browser|'s
  // profile will be used. Testing may require a specific |open_tabs_delegate|.
  RecentTabsSubMenuModel(ui::AcceleratorProvider* accelerator_provider,
                         Browser* browser,
                         sync_sessions::OpenTabsUIDelegate* open_tabs_delegate);
  ~RecentTabsSubMenuModel() override;

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  const gfx::FontList* GetLabelFontListAt(int index) const override;

  int GetMaxWidthForItemAtIndex(int item_index) const;
  bool GetURLAndTitleForItemAtIndex(int index,
                                    std::string* url,
                                    base::string16* title);

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
                         const base::string16& title,
                         const GURL& url,
                         int curr_model_index);

  // Build the recently closed window item with parameters needed to restore it,
  // and add it to the menumodel at |curr_model_index|.
  void BuildLocalWindowItem(const SessionID::id_type& window_id,
                            int num_tabs,
                            int curr_model_index);

  // Build the tab item for other devices with parameters needed to restore it.
  void BuildOtherDevicesTabItem(const std::string& session_tag,
                                const sessions::SessionTab& tab);

  // Add the favicon for the device section header.
  void AddDeviceFavicon(int index_in_menu,
                        sync_sessions::SyncedSession::DeviceType device_type);

  // Add the favicon for a local or other devices' tab asynchronously,
  // OnFaviconDataAvailable() will be invoked when the favicon is ready.
  void AddTabFavicon(int command_id, const GURL& url);
  void OnFaviconDataAvailable(
      int command_id,
      const favicon_base::FaviconImageResult& image_result);

  // Clear all recently closed tabs and windows.
  void ClearLocalEntries();

  // Converts |command_id| of menu item to index in local or other devices'
  // TabNavigationItems, and returns the corresponding local or other devices'
  // TabNavigationItems in |tab_items|.
  int CommandIdToTabVectorIndex(int command_id, TabNavigationItems** tab_items);

  // Used to access (and lazily initialize) open_tabs_delegate_.
  // TODO(tim): This lazy-init for member variables is error prone because you
  // can always skip going through the function and access the field directly.
  // Consider instead having code just deal with potentially NULL open_tabs_
  // and have it initialized by an event / callback.
  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate();

  // Overridden from TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

  Browser* browser_;  // Weak.

  sync_sessions::OpenTabsUIDelegate* open_tabs_delegate_;  // Weak.

  // Accelerator for reopening last closed tab.
  ui::Accelerator reopen_closed_tab_accelerator_;

  // Accelerator for showing history.
  ui::Accelerator show_history_accelerator_;

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

  base::CancelableTaskTracker local_tab_cancelable_task_tracker_;
  base::CancelableTaskTracker other_devices_tab_cancelable_task_tracker_;

  // Time the menu is open for until a recent tab is selected.
  base::ElapsedTimer menu_opened_timer_;

  base::WeakPtrFactory<RecentTabsSubMenuModel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabsSubMenuModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_SUB_MENU_MODEL_H_
