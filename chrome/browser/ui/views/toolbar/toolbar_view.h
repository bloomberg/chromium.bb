// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/toolbar/app_menu_badge_controller.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "components/prefs/pref_member.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

class AppMenuButton;
class BackButton;
class Browser;
class BrowserActionsContainer;
class HomeButton;
class ReloadButton;
class ToolbarButton;

namespace extensions {
class Command;
class Extension;
}

// The Browser Window's toolbar.
class ToolbarView : public views::AccessiblePaneView,
                    public views::MenuButtonListener,
                    public ui::AcceleratorProvider,
                    public LocationBarView::Delegate,
                    public content::NotificationObserver,
                    public CommandObserver,
                    public views::ButtonListener,
                    public views::ViewTargeterDelegate,
                    public AppMenuBadgeController::Delegate {
 public:
  // The view class name.
  static const char kViewClassName[];

  explicit ToolbarView(Browser* browser);
  ~ToolbarView() override;

  // Create the contents of the Browser Toolbar.
  void Init();

  // Forces the toolbar (and transitively the location bar) to update its
  // current state.  If |tab| is non-NULL, we're switching (back?) to this tab
  // and should restore any previous location bar state (such as user editing)
  // as well.
  void Update(content::WebContents* tab);

  // Clears the current state for |tab|.
  void ResetTabState(content::WebContents* tab);

  // Set focus to the toolbar with complete keyboard access, with the
  // focus initially set to the app menu. Focus will be restored
  // to the last focused view if the user escapes.
  void SetPaneFocusAndFocusAppMenu();

  // Returns true if the app menu is focused.
  bool IsAppMenuFocused();

  virtual bool GetAcceleratorInfo(int id, ui::Accelerator* accel);

  // Returns the view to which the bookmark bubble should be anchored.
  views::View* GetBookmarkBubbleAnchor();

  // Returns the view to which the "Save credit card" bubble should be anchored.
  views::View* GetSaveCreditCardBubbleAnchor();

  // Returns the view to which the Translate bubble should be anchored.
  views::View* GetTranslateBubbleAnchor();

  // Adds |anchor_view| as an observer of |bubble_widget| to track its
  // visibility.
  void OnBubbleCreatedForAnchor(views::View* anchor_view,
                                views::Widget* bubble_widget);

  // Returns the maximum width the browser actions container can have.
  int GetMaxBrowserActionsWidth() const;

  // Accessors.
  Browser* browser() const { return browser_; }
  BrowserActionsContainer* browser_actions() const { return browser_actions_; }
  ReloadButton* reload_button() const { return reload_; }
  LocationBarView* location_bar() const { return location_bar_; }
  AppMenuButton* app_menu_button() const { return app_menu_button_; }
  HomeButton* home_button() const { return home_; }
  AppMenuBadgeController* app_menu_badge_controller() {
    return &badge_controller_;
  }

  // AccessiblePaneView:
  bool SetPaneFocus(View* initial_focus) override;
  void GetAccessibleState(ui::AXViewState* state) override;

  // views::MenuButtonListener:
  void OnMenuButtonClicked(views::View* source,
                           const gfx::Point& point) override;

  // LocationBarView::Delegate:
  content::WebContents* GetWebContents() override;
  ToolbarModel* GetToolbarModel() override;
  const ToolbarModel* GetToolbarModel() const override;
  views::Widget* CreateViewsBubble(
      views::BubbleDelegateView* bubble_delegate) override;
  PageActionImageView* CreatePageActionImageView(
      LocationBarView* owner,
      ExtensionAction* action) override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;
  void ShowWebsiteSettings(
      content::WebContents* web_contents,
      const GURL& url,
      const security_state::SecurityStateModel::SecurityInfo& security_info)
      override;

  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;

  // views::View:
  gfx::Size GetPreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  void Layout() override;
  void OnThemeChanged() override;
  const char* GetClassName() const override;
  bool AcceleratorPressed(const ui::Accelerator& acc) override;

 protected:
  // AccessiblePaneView:
  bool SetPaneFocusAndFocusDefault() override;
  void RemovePaneFocus() override;

 private:
  // Types of display mode this toolbar can have.
  enum DisplayMode {
    DISPLAYMODE_NORMAL,       // Normal toolbar with buttons, etc.
    DISPLAYMODE_LOCATION      // Slimline toolbar showing only compact location
                              // bar, used for popups.
  };

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // AppMenuBadgeController::Delegate:
  void UpdateBadgeSeverity(AppMenuBadgeController::BadgeType type,
                           AppMenuIconPainter::Severity severity,
                           bool animate) override;

  // Used to avoid duplicating the near-identical logic of
  // ToolbarView::GetPreferredSize() and ToolbarView::GetMinimumSize(). These
  // two functions call through to GetSizeInternal(), passing themselves as the
  // function pointer |View::*get_size|.
  gfx::Size GetSizeInternal(gfx::Size (View::*get_size)() const) const;

  // Given toolbar contents of size |size|, returns the total toolbar size.
  gfx::Size SizeForContentSize(gfx::Size size) const;

  // Loads the images for all the child views.
  void LoadImages();

  bool is_display_mode_normal() const {
    return display_mode_ == DISPLAYMODE_NORMAL;
  }

  // Shows the critical notification bubble against the app menu.
  void ShowCriticalNotification();

  // Shows the outdated install notification bubble against the app menu.
  // |auto_update_enabled| is set to true when auto-upate is on.
  void ShowOutdatedInstallNotification(bool auto_update_enabled);

  void OnShowHomeButtonChanged();

  int content_shadow_height() const;

  // Controls. Most of these can be null, e.g. in popup windows. Only
  // |location_bar_| is guaranteed to exist.
  BackButton* back_;
  ToolbarButton* forward_;
  ReloadButton* reload_;
  HomeButton* home_;
  LocationBarView* location_bar_;
  BrowserActionsContainer* browser_actions_;
  AppMenuButton* app_menu_button_;

  Browser* browser_;

  AppMenuBadgeController badge_controller_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  // The display mode used when laying out the toolbar.
  const DisplayMode display_mode_;

  content::NotificationRegistrar registrar_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_
