// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_SUB_MENU_MODEL_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
struct SessionTab;

namespace browser_sync {
class SessionModelAssociator;
}

namespace gfx {
class Image;
}

namespace ui {
class AcceleratorProvider;
}

// A menu model that builds the contents of "Recent tabs" submenu, which include
// the last closed tab and opened tabs of other devices.
class RecentTabsSubMenuModel : public ui::SimpleMenuModel,
                               public ui::SimpleMenuModel::Delegate {
 public:
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
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

  int GetMaxWidthForItemAtIndex(int item_index) const;

 private:
  struct NavigationItem;
  typedef std::vector<NavigationItem> NavigationItems;

  // Build the menu items by populating the model.
  void Build();
  void BuildDevices();
  void BuildForeignTabItem(
      const std::string& session_tag,
      const SessionTab& tab);
  void AddDeviceFavicon(int index_in_menu,
                        browser_sync::SyncedSession::DeviceType device_type);
  void AddTabFavicon(int model_index, int command_id, const GURL& url);
  void OnFaviconDataAvailable(int command_id,
                              const history::FaviconImageResult& image_result);
  browser_sync::SessionModelAssociator* GetModelAssociator();

  Browser* browser_;  // Weak.

  browser_sync::SessionModelAssociator* associator_;  // Weak.

  // Accelerator for reopening last closed tab.
  ui::Accelerator reopen_closed_tab_accelerator_;

  // Navigation items.  The |command_id| is set to kFirstTabCommandId plus the
  // index into this vector.  Upon invocation of the menu, the navigation
  // information is retrieved from |model_| and used to navigate to the item
  // specified.
  NavigationItems model_;

  gfx::Image default_favicon_;

  CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<RecentTabsSubMenuModel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabsSubMenuModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_SUB_MENU_MODEL_H_
