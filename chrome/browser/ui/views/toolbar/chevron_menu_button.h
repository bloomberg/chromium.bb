// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHEVRON_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHEVRON_MENU_BUTTON_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"

class BrowserActionsContainer;

// The MenuButton for the chevron in the extension toolbar, which is also
// responsible for showing the legacy (drop-down) overflow menu.
class ChevronMenuButton : public views::MenuButton,
                          public views::MenuButtonListener {
 public:
  explicit ChevronMenuButton(
      BrowserActionsContainer* browser_actions_container);
  ~ChevronMenuButton() override;

  // Closes the overflow menu (and any context menu), if it is open.
  void CloseMenu();

 private:
  class MenuController;

  // views::MenuButton:
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;
  bool GetDropFormats(
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;

  // views::MenuButtonListener:
  void OnMenuButtonClicked(views::MenuButton* source,
                           const gfx::Point& point,
                           const ui::Event* event) override;

  // Shows the overflow menu.
  void ShowOverflowMenu(bool for_drop);

  // Called by the overflow menu when all the work is done.
  void MenuDone();

  // The owning BrowserActionsContainer.
  BrowserActionsContainer* browser_actions_container_;

  // The overflow menu controller.
  std::unique_ptr<MenuController> menu_controller_;

  base::WeakPtrFactory<ChevronMenuButton> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChevronMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHEVRON_MENU_BUTTON_H_
