// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_bubble.h"

#include "ash/login/ui/login_menu_view.h"

namespace ash {

LoginBubble::TestApi::TestApi(LoginBaseBubbleView* bubble_view)
    : bubble_view_(bubble_view) {}

LoginBubble::LoginBubble() = default;

LoginBubble::~LoginBubble() = default;

void LoginBubble::ShowSelectionMenu(LoginMenuView* menu) {
  if (bubble_view_)
    CloseImmediately();

  const bool had_focus =
      menu->GetBubbleOpener() && menu->GetBubbleOpener()->HasFocus();

  // Transfer the ownership of |menu| to bubble widget.
  bubble_view_ = menu;
  bubble_view_->Show();

  if (had_focus) {
    // Try to focus the bubble view only if the bubble opener was focused.
    bubble_view_->RequestFocus();
  }
}

void LoginBubble::Close() {
  if (bubble_view_)
    bubble_view_->Hide();
}

void LoginBubble::CloseImmediately() {
  DCHECK(bubble_view_);

  if (bubble_view_->GetWidget())
    bubble_view_->GetWidget()->Close();

  bubble_view_ = nullptr;
}

bool LoginBubble::IsVisible() {
  return bubble_view_ && bubble_view_->GetWidget()->IsVisible();
}

}  // namespace ash
