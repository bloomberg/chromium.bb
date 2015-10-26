// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_BUTTON_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/toolbar/wrench_icon_painter.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

class AppMenu;
class AppMenuModel;

namespace views {
class InkDropAnimationController;
class LabelButtonBorder;
class MenuListener;
}

class ToolbarView;

class AppMenuButton : public views::InkDropHost,
                      public views::MenuButton,
                      public WrenchIconPainter::Delegate {
 public:
  explicit AppMenuButton(ToolbarView* toolbar_view);
  ~AppMenuButton() override;

  void SetSeverity(WrenchIconPainter::Severity severity, bool animate);

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

  // WrenchIconPainter::Delegate:
  void ScheduleWrenchIconPaint() override;

  // Updates the presentation according to |severity_| and the theme provider.
  // Only used in MD.
  void UpdateIcon();

  // Opens the app menu immediately during a drag-and-drop operation.
  // Used only in testing.
  static bool g_open_app_immediately_for_testing;

 private:
  // views::InkDropHost:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;

  // views::MenuButton:
  const char* GetClassName() const override;
  bool GetDropFormats(
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void Layout() override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;

  // Only used in pre-MD.
  scoped_ptr<WrenchIconPainter> wrench_icon_painter_;

  // Only used in MD.
  WrenchIconPainter::Severity severity_;

  // Animation controller for the ink drop ripple effect.
  scoped_ptr<views::InkDropAnimationController> ink_drop_animation_controller_;

  // Our owning toolbar view.
  ToolbarView* toolbar_view_;

  // Whether or not we should allow dragging extension icons onto this button
  // (in order to open the overflow in the app menu).
  bool allow_extension_dragging_;

  // Listeners to call when the menu opens.
  base::ObserverList<views::MenuListener> menu_listeners_;

  // App model and menu.
  // Note that the menu should be destroyed before the model it uses, so the
  // menu should be listed later.
  scoped_ptr<AppMenuModel> menu_model_;
  scoped_ptr<AppMenu> menu_;

  // Used by ShowMenu() to detect when |this| has been deleted; see comments
  // there.
  bool* destroyed_;

  // Used to spawn weak pointers for delayed tasks to open the overflow menu.
  base::WeakPtrFactory<AppMenuButton> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_BUTTON_H_
