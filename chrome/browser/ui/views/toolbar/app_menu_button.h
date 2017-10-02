// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_BUTTON_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "ui/views/controls/animated_icon_view.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

class AppMenu;
class AppMenuModel;

namespace views {
class LabelButtonBorder;
class MenuListener;
}

class ToolbarView;

// The app menu button lives in the top right of the main browser window. It
// shows three dots and animates to a hamburger-ish icon when there's a need to
// alert the user. Clicking displays the app menu.
class AppMenuButton : public views::MenuButton, public TabStripModelObserver {
 public:
  explicit AppMenuButton(ToolbarView* toolbar_view);
  ~AppMenuButton() override;

  void SetSeverity(AppMenuIconController::IconType type,
                   AppMenuIconController::Severity severity,
                   bool animate);

  // Shows the app menu. |for_drop| indicates whether the menu is opened for a
  // drag-and-drop operation.
  void ShowMenu(bool for_drop);

  // Closes the app menu, if it's open.
  void CloseMenu();

  AppMenu* app_menu_for_testing() { return menu_.get(); }

  // Whether the app/hotdogs menu is currently showing.
  bool IsMenuShowing() const;

  // Adds a listener to receive a callback when the menu opens.
  void AddMenuListener(views::MenuListener* listener);

  // Removes a menu listener.
  void RemoveMenuListener(views::MenuListener* listener);

  // views::MenuButton:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void OnThemeChanged() override;

  // TabStripObserver:
  void TabInsertedAt(TabStripModel* tab_strip_model,
                     content::WebContents* contents,
                     int index,
                     bool foreground) override;

  // Updates the presentation according to |severity_| and the theme provider.
  // If |should_animate| is true, the icon should animate.
  void UpdateIcon(bool should_animate);

  // Sets |margin_trailing_| when the browser is maximized and updates layout
  // to make the focus rectangle centered.
  void SetTrailingMargin(int margin);

  // Animates the icon if possible. The icon will not animate if the severity
  // level is none, |animation_| is nullptr or |should_use_new_icon_| is false.
  void AnimateIconIfPossible();

  // Opens the app menu immediately during a drag-and-drop operation.
  // Used only in testing.
  static bool g_open_app_immediately_for_testing;

 private:
  // views::MenuButton:
  const char* GetClassName() const override;
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;
  gfx::Rect GetThemePaintRect() const override;
  bool GetDropFormats(
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;

  AppMenuIconController::Severity severity_ =
      AppMenuIconController::Severity::NONE;
  AppMenuIconController::IconType type_ = AppMenuIconController::IconType::NONE;

  // Our owning toolbar view.
  ToolbarView* toolbar_view_;

  // Listeners to call when the menu opens.
  base::ObserverList<views::MenuListener> menu_listeners_;

  // App model and menu.
  // Note that the menu should be destroyed before the model it uses, so the
  // menu should be listed later.
  std::unique_ptr<AppMenuModel> menu_model_;
  std::unique_ptr<AppMenu> menu_;

  // The view that depicts and animates the icon. TODO(estade): rename to
  // |animated_icon_| when |should_use_new_icon_| defaults to true and is
  // removed.
  views::AnimatedIconView* new_icon_ = nullptr;

  // True if the app menu should use the new animated icon.
  bool should_use_new_icon_ = false;

  // Any trailing margin to be applied. Used when the browser is in
  // a maximized state to extend to the full window width.
  int margin_trailing_ = 0;

  // Used to spawn weak pointers for delayed tasks to open the overflow menu.
  base::WeakPtrFactory<AppMenuButton> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_BUTTON_H_
