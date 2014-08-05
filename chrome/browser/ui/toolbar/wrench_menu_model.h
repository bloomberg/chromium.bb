// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_WRENCH_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_WRENCH_MENU_MODEL_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/base/models/simple_menu_model.h"

class BookmarkSubMenuModel;
class Browser;
class RecentTabsSubMenuModel;
class TabStripModel;

namespace {
class MockWrenchMenuModel;
}  // namespace

// A menu model that builds the contents of an encoding menu.
class EncodingMenuModel : public ui::SimpleMenuModel,
                          public ui::SimpleMenuModel::Delegate {
 public:
  explicit EncodingMenuModel(Browser* browser);
  virtual ~EncodingMenuModel();

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  void Build();

  Browser* browser_;  // weak

  DISALLOW_COPY_AND_ASSIGN(EncodingMenuModel);
};

// A menu model that builds the contents of the zoom menu.
class ZoomMenuModel : public ui::SimpleMenuModel {
 public:
  explicit ZoomMenuModel(ui::SimpleMenuModel::Delegate* delegate);
  virtual ~ZoomMenuModel();

 private:
  void Build();

  DISALLOW_COPY_AND_ASSIGN(ZoomMenuModel);
};

class ToolsMenuModel : public ui::SimpleMenuModel {
 public:
  ToolsMenuModel(ui::SimpleMenuModel::Delegate* delegate, Browser* browser);
  virtual ~ToolsMenuModel();

 private:
  void Build(Browser* browser);

  scoped_ptr<EncodingMenuModel> encoding_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(ToolsMenuModel);
};

// A menu model that builds the contents of the wrench menu.
class WrenchMenuModel : public ui::SimpleMenuModel,
                        public ui::SimpleMenuModel::Delegate,
                        public ui::ButtonMenuItemModel::Delegate,
                        public TabStripModelObserver,
                        public content::NotificationObserver {
 public:
  // Range of command ID's to use for the items representing bookmarks in the
  // bookmark menu, must not overlap with that for recent tabs submenu.
  static const int kMinBookmarkCommandId = 1;
  static const int kMaxBookmarkCommandId = 1000;

  // Range of command ID's to use for the items in the recent tabs submenu, must
  // not overlap with that for bookmarks.
  static const int kMinRecentTabsCommandId = 1001;
  static const int kMaxRecentTabsCommandId = 1200;

  WrenchMenuModel(ui::AcceleratorProvider* provider, Browser* browser);
  virtual ~WrenchMenuModel();

  // Overridden for ButtonMenuItemModel::Delegate:
  virtual bool DoesCommandIdDismissMenu(int command_id) const OVERRIDE;

  // Overridden for both ButtonMenuItemModel::Delegate and SimpleMenuModel:
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual base::string16 GetLabelForCommandId(int command_id) const OVERRIDE;
  virtual bool GetIconForCommandId(int command_id,
                                   gfx::Image* icon) const OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool IsCommandIdVisible(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                int reason) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index) OVERRIDE;
  virtual void TabStripModelDeleted() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Getters.
  Browser* browser() const { return browser_; }

  BookmarkSubMenuModel* bookmark_sub_menu_model() const {
    return bookmark_sub_menu_model_.get();
  }

  // Calculates |zoom_label_| in response to a zoom change.
  void UpdateZoomControls();

 private:
  class HelpMenuModel;
  // Testing constructor used for mocking.
  friend class ::MockWrenchMenuModel;

  WrenchMenuModel();

  void Build();

  void AddGlobalErrorMenuItems();

  // Appends everything needed for the clipboard menu: a menu break, the
  // clipboard menu content and the finalizing menu break.
  void CreateCutCopyPasteMenu();

  // Add a menu item for the extension icons.
  void CreateExtensionToolbarOverflowMenu();

  // Appends everything needed for the zoom menu: a menu break, then the zoom
  // menu content and then another menu break.
  void CreateZoomMenu();

  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

  bool ShouldShowNewIncognitoWindowMenuItem();

  // Models for the special menu items with buttons.
  scoped_ptr<ui::ButtonMenuItemModel> edit_menu_item_model_;
  scoped_ptr<ui::ButtonMenuItemModel> zoom_menu_item_model_;

  // Label of the zoom label in the zoom menu item.
  base::string16 zoom_label_;

#if defined(GOOGLE_CHROME_BUILD)
  // Help menu.
  scoped_ptr<HelpMenuModel> help_menu_model_;
#endif

  // Tools menu.
  scoped_ptr<ToolsMenuModel> tools_menu_model_;

  // Bookmark submenu.
  scoped_ptr<BookmarkSubMenuModel> bookmark_sub_menu_model_;

  // Recent Tabs submenu.
  scoped_ptr<RecentTabsSubMenuModel> recent_tabs_sub_menu_model_;

  ui::AcceleratorProvider* provider_;  // weak

  Browser* browser_;  // weak
  TabStripModel* tab_strip_model_; // weak

  scoped_ptr<content::HostZoomMap::Subscription> content_zoom_subscription_;
  scoped_ptr<content::HostZoomMap::Subscription> browser_zoom_subscription_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WrenchMenuModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_WRENCH_MENU_MODEL_H_
