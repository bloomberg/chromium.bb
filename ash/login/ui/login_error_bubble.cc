// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_error_bubble.h"

#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// Vertical spacing between the anchor view and error bubble.
constexpr int kAnchorViewErrorBubbleVerticalSpacingDp = 48;

// The size of the alert icon in the error bubble.
constexpr int kAlertIconSizeDp = 20;

// Margin/inset of the entries for the user menu.
constexpr int kUserMenuMarginWidth = 14;
constexpr int kUserMenuMarginHeight = 18;

// Spacing between the child view inside the bubble view.
constexpr int kBubbleBetweenChildSpacingDp = 6;

}  // namespace

// static
LoginErrorBubble* LoginErrorBubble::CreateDefault() {
  aura::Window* menu_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_MenuContainer);
  return new LoginErrorBubble(nullptr /* content */, nullptr /*anchor_view*/,
                              menu_container /*parent_container*/,
                              false /*is_persistent*/);
}

LoginErrorBubble::LoginErrorBubble(views::View* content,
                                   views::View* anchor_view,
                                   aura::Window* parent_container,
                                   bool is_persistent)
    : LoginBaseBubbleView(anchor_view, parent_container),
      is_persistent_(is_persistent) {
  set_anchor_view_insets(
      gfx::Insets(kAnchorViewErrorBubbleVerticalSpacingDp, 0));

  gfx::Insets margins(kUserMenuMarginHeight, kUserMenuMarginWidth);

  set_margins(gfx::Insets(0, margins.left(), 0, margins.right()));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      gfx::Insets(margins.top(), 0, margins.bottom(), 0),
      kBubbleBetweenChildSpacingDp));

  auto* alert_view = new NonAccessibleView("AlertIconContainer");
  alert_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));
  views::ImageView* alert_icon = new views::ImageView();
  alert_icon->SetPreferredSize(gfx::Size(kAlertIconSizeDp, kAlertIconSizeDp));
  alert_icon->SetImage(
      gfx::CreateVectorIcon(kLockScreenAlertIcon, SK_ColorWHITE));
  alert_view->AddChildView(alert_icon);
  AddChildView(alert_view);

  if (content) {
    content_ = content;
    AddChildView(content);
  }
}

LoginErrorBubble::~LoginErrorBubble() = default;

void LoginErrorBubble::SetContent(views::View* content) {
  if (content_)
    RemoveChildView(content_);

  content_ = content;
  AddChildView(content_);
}

bool LoginErrorBubble::IsPersistent() const {
  return is_persistent_;
}

void LoginErrorBubble::SetPersistent(bool persistent) {
  is_persistent_ = persistent;
}

const char* LoginErrorBubble::GetClassName() const {
  return "LoginErrorBubble";
}

void LoginErrorBubble::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTooltip;
}

}  // namespace ash
