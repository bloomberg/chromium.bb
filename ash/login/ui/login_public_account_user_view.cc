// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_public_account_user_view.h"

#include <memory>

#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/layout_util.h"
#include "ash/login/ui/login_display_style.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

constexpr char kLoginPublicAccountUserViewClassName[] =
    "LoginPublicAccountUserView";

// Color of the user domain text.
constexpr SkColor kArrowButtonBackground =
    SkColorSetARGB(0x2B, 0xFF, 0xFF, 0xFF);

// Distance from the top of the user view to the user icon.
constexpr int kDistanceFromTopOfBigUserViewToUserIconDp = 54;

// Distance from the top of the user view to the user icon.
constexpr int kDistanceFromUserViewToArrowButton = 44;

// Distance from the end of arrow button to the bottom of the big user view.
constexpr int kDistanceFromArrowButtonToBigUserViewBottom = 20;

constexpr int kArrowButtonSizeDp = 40;

// Non-empty width, useful for debugging/visualization.
constexpr int kNonEmptyWidth = 1;

constexpr int kArrowButtonFadeAnimationDurationMs = 180;

}  // namespace

LoginPublicAccountUserView::Callbacks::Callbacks() = default;

LoginPublicAccountUserView::Callbacks::Callbacks(const Callbacks& other) =
    default;

LoginPublicAccountUserView::Callbacks::~Callbacks() = default;

LoginPublicAccountUserView::LoginPublicAccountUserView(
    const mojom::LoginUserInfoPtr& user,
    const Callbacks& callbacks)
    : NonAccessibleView(kLoginPublicAccountUserViewClassName),
      on_tap_(callbacks.on_tap),
      on_public_account_tap_(callbacks.on_public_account_tapped) {
  DCHECK_EQ(user->basic_user_info->type,
            user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  DCHECK(callbacks.on_tap);
  DCHECK(callbacks.on_public_account_tapped);

  user_view_ = new LoginUserView(
      LoginDisplayStyle::kLarge, false /*show_dropdown*/, true /*show_domain*/,
      base::BindRepeating(&LoginPublicAccountUserView::OnUserViewTap,
                          base::Unretained(this)),
      base::RepeatingClosure(), base::RepeatingClosure());
  arrow_button_ = new ArrowButtonView(this, kArrowButtonSizeDp);
  arrow_button_->SetBackgroundColor(kArrowButtonBackground);
  arrow_button_->SetFocusPainter(nullptr);

  SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);

  // build layout for public account.
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  views::View* wrapped_user_view =
      login_layout_util::WrapViewForPreferredSize(user_view_);

  auto add_padding = [&](int amount) {
    auto* padding = new NonAccessibleView();
    padding->SetPreferredSize(gfx::Size(kNonEmptyWidth, amount));
    AddChildView(padding);
  };

  add_padding(kDistanceFromTopOfBigUserViewToUserIconDp);
  AddChildView(wrapped_user_view);
  add_padding(kDistanceFromUserViewToArrowButton);
  AddChildView(arrow_button_);
  add_padding(kDistanceFromArrowButtonToBigUserViewBottom);

  // Update authentication UI.
  SetAuthEnabled(auth_enabled_, false /*animate*/);
  user_view_->UpdateForUser(user, false /*animate*/);

  hover_notifier_ = std::make_unique<HoverNotifier>(
      this, base::BindRepeating(&LoginPublicAccountUserView::OnHover,
                                base::Unretained(this)));
}

LoginPublicAccountUserView::~LoginPublicAccountUserView() = default;

void LoginPublicAccountUserView::SetAuthEnabled(bool enabled, bool animate) {
  auth_enabled_ = enabled;
  ignore_hover_ = enabled;
  UpdateArrowButtonOpacity(enabled ? 1 : 0, animate);
  arrow_button_->SetFocusBehavior(enabled ? FocusBehavior::ALWAYS
                                          : FocusBehavior::NEVER);

  // Only the active auth user view has auth enabled. If that is the
  // case, then render the user view as if it was always focused, since clicking
  // on it will not do anything (such as swapping users).
  user_view_->SetForceOpaque(enabled);
  user_view_->SetTapEnabled(!enabled);
  if (enabled)
    arrow_button_->RequestFocus();
}

void LoginPublicAccountUserView::UpdateForUser(
    const mojom::LoginUserInfoPtr& user) {
  user_view_->UpdateForUser(user, true /*animate*/);
}

const mojom::LoginUserInfoPtr& LoginPublicAccountUserView::current_user()
    const {
  return user_view_->current_user();
}

gfx::Size LoginPublicAccountUserView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  // Make sure we are at least as big as the user view. If we do not do this the
  // view will be below minimum size when no auth methods are displayed.
  size.SetToMax(user_view_->GetPreferredSize());
  return size;
}

void LoginPublicAccountUserView::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender == arrow_button_) {
    DCHECK(arrow_button_);
    on_public_account_tap_.Run();
  }
}

void LoginPublicAccountUserView::OnUserViewTap() {
  on_tap_.Run();
}

void LoginPublicAccountUserView::OnHover(bool has_hover) {
  if (!ignore_hover_)
    UpdateArrowButtonOpacity(has_hover ? 1 : 0, true /*animate*/);
}

void LoginPublicAccountUserView::UpdateArrowButtonOpacity(float target_opacity,
                                                          bool animate) {
  if (!animate) {
    arrow_button_->layer()->SetOpacity(target_opacity);
    return;
  }

  // Set up animation.
  {
    ui::ScopedLayerAnimationSettings settings(
        arrow_button_->layer()->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kArrowButtonFadeAnimationDurationMs));
    settings.SetTweenType(gfx::Tween::EASE_IN_OUT);

    arrow_button_->layer()->SetOpacity(target_opacity);
  }
}

}  // namespace ash
