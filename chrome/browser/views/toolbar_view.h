// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_

#include <vector>

#include "app/menus/simple_menu_model.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/app_menu_model.h"
#include "chrome/browser/back_forward_menu_model_views.h"
#include "chrome/browser/bubble_positioner.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/page_menu_model.h"
#include "chrome/browser/views/accessible_toolbar_view.h"
#include "chrome/browser/views/go_button.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/pref_member.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

class BrowserActionsContainer;
class Browser;
class Profile;
class ToolbarStarToggle;

namespace views {
class Menu2;
}

// The Browser Window's toolbar.
class ToolbarView : public AccessibleToolbarView,
                    public views::ViewMenuDelegate,
                    public views::DragController,
                    public menus::SimpleMenuModel::Delegate,
                    public LocationBarView::Delegate,
                    public NotificationObserver,
                    public CommandUpdater::CommandObserver,
                    public views::ButtonListener,
                    public BubblePositioner {
 public:
  explicit ToolbarView(Browser* browser);
  virtual ~ToolbarView() { }

  // Create the contents of the Browser Toolbar
  void Init(Profile* profile);

  // Sets the profile which is active on the currently-active tab.
  void SetProfile(Profile* profile);
  Profile* profile() { return profile_; }

  // Updates the toolbar (and transitively the location bar) with the states of
  // the specified |tab|.  If |should_restore_state| is true, we're switching
  // (back?) to this tab and should restore any previous location bar state
  // (such as user editing) as well.
  void Update(TabContents* tab, bool should_restore_state);

  // Accessors...
  Browser* browser() const { return browser_; }
  BrowserActionsContainer* browser_actions() const { return browser_actions_; }
  ToolbarStarToggle* star_button() const { return star_; }
  GoButton* go_button() const { return go_; }
  LocationBarView* location_bar() const { return location_bar_; }
  views::MenuButton* page_menu() const { return page_menu_; }
  views::MenuButton* app_menu() const { return app_menu_; }

  // Overridden from AccessibleToolbarView:
  virtual bool IsAccessibleViewTraversable(views::View* view);

  // Overridden from Menu::BaseControllerDelegate:
  virtual bool GetAcceleratorInfo(int id, menus::Accelerator* accel);

  // Overridden from views::MenuDelegate:
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Overridden from LocationBarView::Delegate:
  virtual TabContents* GetTabContents();
  virtual void OnInputInProgress(bool in_progress);

  // Overridden from CommandUpdater::CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled);

  // Overridden from views::BaseButton::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // BubblePositioner:
  virtual gfx::Rect GetLocationStackBounds() const;

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from menus::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual void ThemeChanged();

 private:
  // Overridden from views::DragController:
  virtual void WriteDragData(View* sender,
                             int press_x,
                             int press_y,
                             OSExchangeData* data);
  virtual int GetDragOperations(View* sender, int x, int y);
  virtual bool CanStartDrag(View* sender,
                            int press_x,
                            int press_y,
                            int x,
                            int y) {
    return true;
  }

  // Returns the number of pixels above the location bar in non-normal display.
  int PopupTopSpacing() const;

  // Set up the various Views in the toolbar
  void CreateLeftSideControls();
  void CreateCenterStack(Profile* profile);
  void CreateRightSideControls(Profile* profile);
  void LoadLeftSideControlsImages();
  void LoadCenterStackImages();
  void LoadRightSideControlsImages();

  // Runs various menus.
  void RunPageMenu(const gfx::Point& pt);
  void RunAppMenu(const gfx::Point& pt);

  // Types of display mode this toolbar can have.
  enum DisplayMode {
    DISPLAYMODE_NORMAL,       // Normal toolbar with buttons, etc.
    DISPLAYMODE_LOCATION      // Slimline toolbar showing only compact location
                              // bar, used for popups.
  };
  bool IsDisplayModeNormal() const {
    return display_mode_ == DISPLAYMODE_NORMAL;
  }

  scoped_ptr<BackForwardMenuModelViews> back_menu_model_;
  scoped_ptr<BackForwardMenuModelViews> forward_menu_model_;

  // The model that contains the security level, text, icon to display...
  ToolbarModel* model_;

  // Controls
  views::ImageButton* back_;
  views::ImageButton* forward_;
  views::ImageButton* reload_;
  views::ImageButton* home_;
  ToolbarStarToggle* star_;
  LocationBarView* location_bar_;
  GoButton* go_;
  BrowserActionsContainer* browser_actions_;
  views::MenuButton* page_menu_;
  views::MenuButton* app_menu_;
  // The bookmark menu button. This may be null.
  views::MenuButton* bookmark_menu_;
  Profile* profile_;
  Browser* browser_;

  // Contents of the profiles menu to populate with profile names.
  scoped_ptr<menus::SimpleMenuModel> profiles_menu_contents_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  // The display mode used when laying out the toolbar.
  DisplayMode display_mode_;

  // The contents of the various menus.
  scoped_ptr<PageMenuModel> page_menu_model_;
  scoped_ptr<AppMenuModel> app_menu_model_;

  // TODO(beng): build these into MenuButton.
  scoped_ptr<views::Menu2> page_menu_menu_;
  scoped_ptr<views::Menu2> app_menu_menu_;
};

#endif  // CHROME_BROWSER_VIEWS_TOOLBAR_VIEW_H_
