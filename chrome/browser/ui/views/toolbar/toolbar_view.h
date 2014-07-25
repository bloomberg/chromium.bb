// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/toolbar/wrench_menu_badge_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

class BackButton;
class BrowserActionsContainer;
class Browser;
class HomeButton;
class ReloadButton;
class ToolbarButton;
class WrenchMenu;
class WrenchMenuModel;
class WrenchToolbarButton;

namespace extensions {
class Command;
class Extension;
class ExtensionMessageBubbleFactory;
}

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
                    public views::WidgetObserver,
                    public views::ViewTargeterDelegate,
                    public WrenchMenuBadgeController::Delegate {
 public:
  // The view class name.
  static const char kViewClassName[];

  explicit ToolbarView(Browser* browser);
  virtual ~ToolbarView();

  // Create the contents of the Browser Toolbar.
  void Init();

  // Forces the toolbar (and transitively the location bar) to update its
  // current state.  If |tab| is non-NULL, we're switching (back?) to this tab
  // and should restore any previous location bar state (such as user editing)
  // as well.
  void Update(content::WebContents* tab);

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

  virtual bool GetAcceleratorInfo(int id, ui::Accelerator* accel);

  // Returns the view to which the bookmark bubble should be anchored.
  views::View* GetBookmarkBubbleAnchor();

  // Returns the view to which the Translate bubble should be anchored.
  views::View* GetTranslateBubbleAnchor();

  // Executes |command| registered by |extension|.
  void ExecuteExtensionCommand(const extensions::Extension* extension,
                               const extensions::Command& command);

  // Shows the extension's page action, if present.
  void ShowPageActionPopup(const extensions::Extension* extension);

  // Shows the extension's browser action, if present.
  void ShowBrowserActionPopup(const extensions::Extension* extension);

  // Shows the app (wrench) menu. |for_drop| indicates whether the menu is
  // opened for a drag-and-drop operation.
  void ShowAppMenu(bool for_drop);

  // Accessors.
  Browser* browser() const { return browser_; }
  BrowserActionsContainer* browser_actions() const { return browser_actions_; }
  ReloadButton* reload_button() const { return reload_; }
  LocationBarView* location_bar() const { return location_bar_; }
  views::MenuButton* app_menu() const;
  HomeButton* home_button() const { return home_; }

  // AccessiblePaneView:
  virtual bool SetPaneFocus(View* initial_focus) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;

  // views::MenuButtonListener:
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  // LocationBarView::Delegate:
  virtual content::WebContents* GetWebContents() OVERRIDE;
  virtual ToolbarModel* GetToolbarModel() OVERRIDE;
  virtual const ToolbarModel* GetToolbarModel() const OVERRIDE;
  virtual InstantController* GetInstant() OVERRIDE;
  virtual views::Widget* CreateViewsBubble(
      views::BubbleDelegateView* bubble_delegate) OVERRIDE;
  virtual PageActionImageView* CreatePageActionImageView(
      LocationBarView* owner, ExtensionAction* action) OVERRIDE;
  virtual ContentSettingBubbleModelDelegate*
      GetContentSettingBubbleModelDelegate() OVERRIDE;
  virtual void ShowWebsiteSettings(content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl) OVERRIDE;

  // CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::WidgetObserver:
  virtual void OnWidgetVisibilityChanged(views::Widget* widget,
                                         bool visible) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ui::AcceleratorProvider:
  virtual bool GetAcceleratorForCommandId(
      int command_id, ui::Accelerator* accelerator) OVERRIDE;

  // views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& acc) OVERRIDE;

  // Whether the wrench/hotdogs menu is currently showing.
  bool IsWrenchMenuShowing() const;

  // Whether the toolbar view needs its background painted by the
  // BrowserNonClientFrameView.
  bool ShouldPaintBackground() const;

  enum {
    // The apparent horizontal space between most items, and the vertical
    // padding above and below them.
    kStandardSpacing = 3,

    // The top of the toolbar has an edge we have to skip over in addition to
    // the standard spacing.
    kVertSpacing = 5,
  };

 protected:
  // AccessiblePaneView:
  virtual bool SetPaneFocusAndFocusDefault() OVERRIDE;
  virtual void RemovePaneFocus() OVERRIDE;

 private:
  // Types of display mode this toolbar can have.
  enum DisplayMode {
    DISPLAYMODE_NORMAL,       // Normal toolbar with buttons, etc.
    DISPLAYMODE_LOCATION      // Slimline toolbar showing only compact location
                              // bar, used for popups.
  };

  // views::ViewTargeterDelegate:
  virtual bool DoesIntersectRect(const views::View* target,
                                 const gfx::Rect& rect) const OVERRIDE;

  // WrenchMenuBadgeController::Delegate:
  virtual void UpdateBadgeSeverity(WrenchMenuBadgeController::BadgeType type,
                                   WrenchIconPainter::Severity severity,
                                   bool animate) OVERRIDE;

  // Returns the number of pixels above the location bar in non-normal display.
  int PopupTopSpacing() const;

  // Given toolbar contents of size |size|, returns the total toolbar size.
  gfx::Size SizeForContentSize(gfx::Size size) const;

  // Loads the images for all the child views.
  void LoadImages();

  bool is_display_mode_normal() const {
    return display_mode_ == DISPLAYMODE_NORMAL;
  }

  // Shows the critical notification bubble against the wrench menu.
  void ShowCriticalNotification();

  // Shows the outdated install notification bubble against the wrench menu.
  // |auto_update_enabled| is set to true when auto-upate is on.
  void ShowOutdatedInstallNotification(bool auto_update_enabled);

  void OnShowHomeButtonChanged();

  int content_shadow_height() const;

  // Controls
  BackButton* back_;
  ToolbarButton* forward_;
  ReloadButton* reload_;
  HomeButton* home_;
  LocationBarView* location_bar_;
  BrowserActionsContainer* browser_actions_;
  WrenchToolbarButton* app_menu_;
  Browser* browser_;

  WrenchMenuBadgeController badge_controller_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  // The display mode used when laying out the toolbar.
  DisplayMode display_mode_;

  // Wrench model and menu.
  // Note that the menu should be destroyed before the model it uses, so the
  // menu should be listed later.
  scoped_ptr<WrenchMenuModel> wrench_menu_model_;
  scoped_ptr<WrenchMenu> wrench_menu_;

  // The factory to create bubbles to warn about dangerous/suspicious
  // extensions.
  scoped_ptr<extensions::ExtensionMessageBubbleFactory>
      extension_message_bubble_factory_;

  // A list of listeners to call when the menu opens.
  ObserverList<views::MenuListener> menu_listeners_;

  content::NotificationRegistrar registrar_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_
