// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_TOOLBAR_BUTTON_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/toolbar/wrench_icon_painter.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

class WrenchMenu;
class WrenchMenuModel;

namespace views {
class LabelButtonBorder;
class MenuListener;
}

class ToolbarView;

// TODO(gbillock): Rename this? No longer a wrench.
class WrenchToolbarButton : public views::MenuButton,
                            public WrenchIconPainter::Delegate {
 public:
  explicit WrenchToolbarButton(ToolbarView* toolbar_view);
  ~WrenchToolbarButton() override;

  void SetSeverity(WrenchIconPainter::Severity severity, bool animate);

  // Shows the app menu. |for_drop| indicates whether the menu is opened for a
  // drag-and-drop operation.
  void ShowMenu(bool for_drop);

  // Closes the app menu, if it's open.
  void CloseMenu();

  WrenchMenu* app_menu_for_testing() { return menu_.get(); }

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

  // Opens the app menu immediately during a drag-and-drop operation.
  // Used only in testing.
  static bool g_open_app_immediately_for_testing;

 private:
  // views::MenuButton:
  const char* GetClassName() const override;
  bool GetDropFormats(
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;

  scoped_ptr<WrenchIconPainter> wrench_icon_painter_;

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
  scoped_ptr<WrenchMenuModel> menu_model_;
  scoped_ptr<WrenchMenu> menu_;

  // Used to spawn weak pointers for delayed tasks to open the overflow menu.
  base::WeakPtrFactory<WrenchToolbarButton> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WrenchToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_TOOLBAR_BUTTON_H_
