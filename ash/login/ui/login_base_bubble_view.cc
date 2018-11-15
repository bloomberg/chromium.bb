// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_base_bubble_view.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// Total width of the bubble view.
constexpr int kBubbleTotalWidthDp = 178;

// Horizontal margin of the bubble view.
constexpr int kBubbleHorizontalMarginDp = 14;

// Top margin of the bubble view.
constexpr int kBubbleTopMarginDp = 13;

// Bottom margin of the bubble view.
constexpr int kBubbleBottomMarginDp = 18;

}  // namespace

LoginBaseBubbleView::LoginBaseBubbleView(views::View* anchor_view)
    : LoginBaseBubbleView(anchor_view, nullptr) {}

LoginBaseBubbleView::LoginBaseBubbleView(views::View* anchor_view,
                                         aura::Window* parent_window)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::NONE) {
  set_margins(gfx::Insets(kBubbleTopMarginDp, kBubbleHorizontalMarginDp,
                          kBubbleBottomMarginDp, kBubbleHorizontalMarginDp));
  set_color(SK_ColorBLACK);
  set_can_activate(false);
  set_close_on_deactivate(false);

  // Layer rendering is needed for animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  set_parent_window(parent_window);
}

LoginBaseBubbleView::~LoginBaseBubbleView() = default;

LoginButton* LoginBaseBubbleView::GetBubbleOpener() const {
  return nullptr;
}

void LoginBaseBubbleView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  // This case only gets called if the bubble has no anchor and no parent
  // container was specified. In this case, the parent container should default
  // to MenuContainer, so that login bubbles are visible over the shelf and
  // virtual keyboard. Shell may be null in tests.
  if (!params->parent && Shell::HasInstance()) {
    params->parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                         kShellWindowId_MenuContainer);
  }
}

int LoginBaseBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

gfx::Size LoginBaseBubbleView::CalculatePreferredSize() const {
  gfx::Size size = views::BubbleDialogDelegateView::CalculatePreferredSize();
  size.set_width(kBubbleTotalWidthDp);
  return size;
}

}  // namespace ash
