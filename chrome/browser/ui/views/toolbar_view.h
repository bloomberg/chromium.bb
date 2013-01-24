// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_VIEW_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/public/pref_member.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/reload_button.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

class BrowserActionsContainer;
class Browser;
class WrenchMenu;
class WrenchMenuModel;

namespace views {
class MenuListener;
}

// The Browser Window's toolbar.
class ToolbarView : public views::AccessiblePaneView,
                    public views::MenuButtonListener,
                    public ui::AcceleratorProvider,
                    public LocationBarView::Delegate,
                    public content::NotificationObserver,
                    public CommandObserver,
                    public views::ButtonListener,
                    public views::WidgetObserver {
 public:
  // The view class name.
  static const char kViewClassName[];

  explicit ToolbarView(Browser* browser);
  virtual ~ToolbarView();

  // Create the contents of the Browser Toolbar.
  void Init();

  // Updates the toolbar (and transitively the location bar) with the states of
  // the specified |tab|.  If |should_restore_state| is true, we're switching
  // (back?) to this tab and should restore any previous location bar state
  // (such as user editing) as well.
  void Update(content::WebContents* tab, bool should_restore_state);

  // Set focus to the toolbar with complete keyboard access, with the
  // focus initially set to the app menu. Focus will be restored
  // to the last focused view if the user escapes.
  void SetPaneFocusAndFocusAppMenu();

  // Returns true if the app menu is focused.
  bool IsAppMenuFocused();

  // Add a listener to receive a callback when the menu opens.
  void AddMenuListener(views::MenuListener* listener);

  // Remove a menu listener.
  void RemoveMenuListener(views::MenuListener* listener);

  // Gets an image with the icon for the app menu and any overlaid notification
  // badge.
  gfx::ImageSkia GetAppMenuIcon(views::CustomButton::ButtonState state);

  virtual bool GetAcceleratorInfo(int id, ui::Accelerator* accel);

  // Returns the view to which the bookmark bubble should be anchored.
  views::View* GetBookmarkBubbleAnchor();

  // Accessors...
  Browser* browser() const { return browser_; }
  BrowserActionsContainer* browser_actions() const { return browser_actions_; }
  ReloadButton* reload_button() const { return reload_; }
  LocationBarView* location_bar() const { return location_bar_; }
  views::MenuButton* app_menu() const { return app_menu_; }

  // Overridden from AccessiblePaneView
  virtual bool SetPaneFocus(View* initial_focus) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Overridden from views::MenuButtonListener:
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  // Overridden from LocationBarView::Delegate:
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual InstantController* GetInstant() OVERRIDE;
  virtual views::Widget* CreateViewsBubble(
      views::BubbleDelegateView* bubble_delegate) OVERRIDE;
  virtual PageActionImageView* CreatePageActionImageView(
      LocationBarView* owner, ExtensionAction* action) OVERRIDE;
  virtual ContentSettingBubbleModelDelegate*
      GetContentSettingBubbleModelDelegate() OVERRIDE;
  virtual void ShowWebsiteSettings(content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl,
                                   bool show_history) OVERRIDE;
  virtual void OnInputInProgress(bool in_progress) OVERRIDE;

  // Overridden from CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetVisibilityChanged(views::Widget* widget,
                                         bool visible) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from ui::AcceleratorProvider:
  virtual bool GetAcceleratorForCommandId(
      int command_id, ui::Accelerator* accelerator) OVERRIDE;

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool GetDropFormats(
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool CanDrop(const ui::OSExchangeData& data) OVERRIDE;
  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE;
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& acc) OVERRIDE;

  // Whether the wrench/hotdogs menu is currently showing.
  bool IsWrenchMenuShowing() const;

  // The apparent horizontal space between most items, and the vertical padding
  // above and below them.
  static const int kStandardSpacing;
  // The top of the toolbar has an edge we have to skip over in addition to the
  // standard spacing.
  static const int kVertSpacing;

 protected:
  // Overridden from AccessiblePaneView
  virtual bool SetPaneFocusAndFocusDefault() OVERRIDE;
  virtual void RemovePaneFocus() OVERRIDE;

 private:
  // Types of display mode this toolbar can have.
  enum DisplayMode {
    DISPLAYMODE_NORMAL,       // Normal toolbar with buttons, etc.
    DISPLAYMODE_LOCATION      // Slimline toolbar showing only compact location
                              // bar, used for popups.
  };

  // Returns true if we should show the upgrade recommended dot.
  bool ShouldShowUpgradeRecommended();

  // Returns true if we should show the background page badge.
  bool ShouldShowBackgroundPageBadge();

  // Returns true if we should show the warning for incompatible software.
  bool ShouldShowIncompatibilityWarning();

  // Returns the number of pixels above the location bar in non-normal display.
  int PopupTopSpacing() const;

  // Loads the images for all the child views.
  void LoadImages();

  bool is_display_mode_normal() const {
    return display_mode_ == DISPLAYMODE_NORMAL;
  }

  // Shows the critical notification bubble against the wrench menu.
  void ShowCriticalNotification();

  // Updates the badge and the accessible name of the app menu (Wrench).
  void UpdateAppMenuState();

  void OnShowHomeButtonChanged();

  // The model that contains the security level, text, icon to display...
  ToolbarModel* model_;

  // Controls
  views::ImageButton* back_;
  views::ImageButton* forward_;
  ReloadButton* reload_;
  views::ImageButton* home_;
  LocationBarView* location_bar_;
  BrowserActionsContainer* browser_actions_;
  views::MenuButton* app_menu_;
  Browser* browser_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  // A pref that counts how often the bubble has been shown.
  IntegerPrefMember sideload_wipeout_bubble_shown_;

  // The display mode used when laying out the toolbar.
  DisplayMode display_mode_;

  // Wrench model and menu.
  // Note that the menu should be destroyed before the model it uses, so the
  // menu should be listed later.
  scoped_ptr<WrenchMenuModel> wrench_menu_model_;
  scoped_ptr<WrenchMenu> wrench_menu_;

  // A list of listeners to call when the menu opens.
  ObserverList<views::MenuListener> menu_listeners_;

  content::NotificationRegistrar registrar_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_VIEW_H_
