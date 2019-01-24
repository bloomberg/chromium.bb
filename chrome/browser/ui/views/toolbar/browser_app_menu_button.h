// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/views/view.h"

class ToolbarView;

// The app menu button in the main browser window (as opposed to hosted app
// windows, which is implemented in HostedAppMenuButton).
class BrowserAppMenuButton : public AppMenuButton,
                             public ui::MaterialDesignControllerObserver {
 public:
  explicit BrowserAppMenuButton(ToolbarView* toolbar_view);
  ~BrowserAppMenuButton() override;

  void SetTypeAndSeverity(
      AppMenuIconController::TypeAndSeverity type_and_severity);

  AppMenuIconController::Severity severity() {
    return type_and_severity_.severity;
  }

  // Shows the app menu. |for_drop| indicates whether the menu is opened for a
  // drag-and-drop operation.
  void ShowMenu(bool for_drop);

  // Sets whether an in-product help feature promo is showing for the app menu.
  // When true, the button is highlighted in a noticeable color.
  void SetPromoIsShowing(bool promo_is_showing);

  // views::MenuButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  gfx::Rect GetAnchorBoundsInScreen() const override;
  void OnThemeChanged() override;

  // Updates the presentation according to |severity_| and the theme provider.
  void UpdateIcon();

  // Sets |margin_trailing_| when the browser is maximized and updates layout
  // to make the focus rectangle centered.
  void SetTrailingMargin(int margin);

  // Opens the app menu immediately during a drag-and-drop operation.
  // Used only in testing.
  static bool g_open_app_immediately_for_testing;

 protected:
  // ui::MaterialDesignControllerObserver:
  void OnTouchUiChanged() override;

 private:
  void UpdateBorder();

  // AppMenuButton:
  const char* GetClassName() const override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;

  AppMenuIconController::TypeAndSeverity type_and_severity_{
      AppMenuIconController::IconType::NONE,
      AppMenuIconController::Severity::NONE};

  // Our owning toolbar view.
  ToolbarView* const toolbar_view_;

  // Whether an in-product help promo is currently showing for the app menu.
  bool promo_is_showing_ = false;

  ScopedObserver<ui::MaterialDesignController,
                 ui::MaterialDesignControllerObserver>
      md_observer_{this};

  // Used to spawn weak pointers for delayed tasks to open the overflow menu.
  base::WeakPtrFactory<BrowserAppMenuButton> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserAppMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_
