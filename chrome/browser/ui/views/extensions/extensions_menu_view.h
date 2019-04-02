// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_

#include <memory>

#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class ImageButton;
}  // namespace views

// This bubble view displays a list of user extensions.
// TODO(pbos): Once there's more functionality in here (getting to
// chrome://extensions, pinning, extension settings), update this comment.
class ExtensionsMenuView : public views::ButtonListener,
                           public views::BubbleDialogDelegateView,
                           public ToolbarActionsModel::Observer {
 public:
  ExtensionsMenuView(views::View* anchor_view, Browser* browser);
  ~ExtensionsMenuView() override;

  static void ShowBubble(views::View* anchor_view, Browser* browser);
  static bool IsShowing();
  static void Hide();
  static ExtensionsMenuView* GetExtensionsMenuViewForTesting();

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::BubbleDialogDelegateView:
  base::string16 GetAccessibleWindowTitle() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  int GetDialogButtons() const override;
  bool ShouldSnapFrameWidth() const override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& item,
                            int index) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionMoved(const ToolbarActionsModel::ActionId& action_id,
                            int index) override;
  void OnToolbarActionLoadFailed() override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarVisibleCountChanged() override;
  void OnToolbarHighlightModeChanged(bool is_highlighting) override;
  void OnToolbarModelInitialized() override;

 private:
  void Repopulate();

  std::unique_ptr<views::ImageButton> CreateImageButtonForHeader(
      const gfx::VectorIcon& icon,
      int id,
      const base::string16& tooltip);

  Browser* const browser_;
  ToolbarActionsModel* const model_;
  ScopedObserver<ToolbarActionsModel, ToolbarActionsModel::Observer>
      model_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_
