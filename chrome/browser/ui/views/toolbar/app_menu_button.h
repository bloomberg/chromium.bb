// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_BUTTON_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
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

class AppMenuButton : public views::MenuButton {
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
  gfx::Size GetPreferredSize() const override;

  // Updates the presentation according to |severity_| and the theme provider.
  void UpdateIcon();

  // Sets |margin_trailing_| when the browser is maximized and updates layout
  // to make the focus rectangle centered.
  void SetTrailingMargin(int margin);

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

  AppMenuIconController::Severity severity_;
  AppMenuIconController::IconType type_;

  // Our owning toolbar view.
  ToolbarView* toolbar_view_;

  // Listeners to call when the menu opens.
  base::ObserverList<views::MenuListener> menu_listeners_;

  // App model and menu.
  // Note that the menu should be destroyed before the model it uses, so the
  // menu should be listed later.
  std::unique_ptr<AppMenuModel> menu_model_;
  std::unique_ptr<AppMenu> menu_;

  // Any trailing margin to be applied. Used when the browser is in
  // a maximized state to extend to the full window width.
  int margin_trailing_;

  // Used to spawn weak pointers for delayed tasks to open the overflow menu.
  base::WeakPtrFactory<AppMenuButton> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_BUTTON_H_
