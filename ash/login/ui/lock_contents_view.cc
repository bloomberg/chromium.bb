// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view.h"

#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_user_view.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Any non-zero value used for separator height. Makes debugging easier; this
// should not affect visual appearance.
constexpr int kNonEmptyHeightDp = 30;

// Horizontal distance between two users in the low density layout.
constexpr int kLowDensityDistanceBetweenUsersDp = 118;

// Margin left of the auth user in the medium density layout.
constexpr int kMediumDensityMarginLeftOfAuthUserDp = 98;

// Horizontal distance between the auth user and the medium density user row.
constexpr int kMediumDensityDistanceBetweenAuthUserAndUsersDp = 220;

// Vertical padding between each entry in the medium density user row
constexpr int kMediumDensityVerticalDistanceBetweenUsersDp = 53;

// Horizontal padding left and right of the high density user list.
constexpr int kHighDensityHorizontalPaddingLeftOfUserListDp = 72;
constexpr int kHighDensityHorizontalPaddingRightOfUserListDp = 72;

// The vertical padding between each entry in the extra-small user row
constexpr int kHighDensityVerticalDistanceBetweenUsersDp = 32;

// Duration (in milliseconds) of the auth user view animation, ie, when enabling
// or disabling the PIN keyboard.
constexpr int kAuthUserViewAnimationDurationMs = 100;

// Builds a view with the given preferred size.
views::View* MakePreferredSizeView(gfx::Size size) {
  auto* view = new views::View();
  view->SetPreferredSize(size);
  return view;
}

}  // namespace

LockContentsView::TestApi::TestApi(LockContentsView* view) : view_(view) {}

LockContentsView::TestApi::~TestApi() = default;

LoginAuthUserView* LockContentsView::TestApi::auth_user_view() const {
  return view_->auth_user_view_;
}

const std::vector<LoginUserView*>& LockContentsView::TestApi::user_views()
    const {
  return view_->user_views_;
}

LockContentsView::UserState::UserState(AccountId account_id)
    : account_id(account_id) {}

LockContentsView::LockContentsView(LoginDataDispatcher* data_dispatcher)
    : data_dispatcher_(data_dispatcher) {
  data_dispatcher_->AddObserver(this);
}

LockContentsView::~LockContentsView() {
  data_dispatcher_->RemoveObserver(this);
}

void LockContentsView::Layout() {
  View::Layout();
  if (scroller_)
    scroller_->ClipHeightTo(size().height(), size().height());
  background_->SetSize(size());
}

void LockContentsView::OnUsersChanged(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  // The debug view will potentially call this method many times. Make sure to
  // invalidate any child references.
  RemoveAllChildViews(true /*delete_children*/);
  user_views_.clear();
  scroller_ = nullptr;

  // Build user state list.
  users_.clear();
  for (const ash::mojom::UserInfoPtr& user : users)
    users_.push_back(UserState{user->account_id});

  // Build view hierarchy.
  background_ = new views::View();
  // TODO(jdufault): Set background color based on wallpaper color.
  background_->SetBackground(
      views::CreateSolidBackground(SkColorSetARGB(80, 0, 0, 0)));
  AddChildView(background_);

  auto* layout = new views::BoxLayout(views::BoxLayout::kHorizontal);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(layout);

  // Add auth user.
  auth_user_view_ =
      new LoginAuthUserView(users[0], base::Bind([](bool auth_success) {
                              if (auth_success)
                                ash::LockScreen::Get()->Destroy();
                            }));
  AddChildView(auth_user_view_);
  auth_user_view_animator_ =
      base::MakeUnique<views::BoundsAnimator>(auth_user_view_->parent());
  auth_user_view_animator_->SetAnimationDuration(
      kAuthUserViewAnimationDurationMs);
  UpdateAuthMethodsForAuthUser(false /*animate*/);

  // Build layout for additional users.
  if (users.size() == 2)
    CreateLowDensityLayout(users);
  else if (users.size() >= 3 && users.size() <= 6)
    CreateMediumDensityLayout(users);
  else if (users.size() >= 7)
    CreateHighDensityLayout(users, layout);

  // Force layout.
  PreferredSizeChanged();
  Layout();
}

void LockContentsView::OnPinEnabledForUserChanged(const AccountId& user,
                                                  bool enabled) {
  LockContentsView::UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user when changing PIN state to " << enabled;
    return;
  }

  state->show_pin = enabled;
  UpdateAuthMethodsForAuthUser(true /*animate*/);
}

void LockContentsView::CreateLowDensityLayout(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  // Space between auth user and alternative user.
  AddChildView(MakePreferredSizeView(
      gfx::Size(kLowDensityDistanceBetweenUsersDp, kNonEmptyHeightDp)));
  auto* alt_user_view =
      new LoginUserView(LoginDisplayStyle::kLarge, false /*show_dropdown*/);
  alt_user_view->UpdateForUser(users[1]);
  user_views_.push_back(alt_user_view);
  AddChildView(alt_user_view);
}

void LockContentsView::CreateMediumDensityLayout(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  // Insert spacing before (left of) auth.
  AddChildViewAt(MakePreferredSizeView(gfx::Size(
                     kMediumDensityMarginLeftOfAuthUserDp, kNonEmptyHeightDp)),
                 0);
  // Insert spacing between auth and user list.
  AddChildView(MakePreferredSizeView(gfx::Size(
      kMediumDensityDistanceBetweenAuthUserAndUsersDp, kNonEmptyHeightDp)));

  // Add additional users.
  auto* row = new views::View();
  AddChildView(row);
  auto* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(),
                           kMediumDensityVerticalDistanceBetweenUsersDp);
  row->SetLayoutManager(layout);
  for (std::size_t i = 1u; i < users.size(); ++i) {
    auto* view =
        new LoginUserView(LoginDisplayStyle::kSmall, false /*show_dropdown*/);
    user_views_.push_back(view);
    view->UpdateForUser(users[i]);
    row->AddChildView(view);
  }
}

void LockContentsView::CreateHighDensityLayout(
    const std::vector<ash::mojom::UserInfoPtr>& users,
    views::BoxLayout* layout) {
  // TODO: Finish 7+ user layout.

  // Insert spacing before and after the auth view.
  auto* fill = new views::View();
  AddChildViewAt(fill, 0);
  layout->SetFlexForView(fill, 1);

  fill = new views::View();
  AddChildView(fill);
  layout->SetFlexForView(fill, 1);

  // Padding left of user list.
  AddChildView(MakePreferredSizeView(gfx::Size(
      kHighDensityHorizontalPaddingLeftOfUserListDp, kNonEmptyHeightDp)));

  // Add user list.
  auto* row = new views::View();
  auto* row_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(),
                           kHighDensityVerticalDistanceBetweenUsersDp);
  row_layout->set_minimum_cross_axis_size(
      LoginUserView::WidthForLayoutStyle(LoginDisplayStyle::kExtraSmall));
  row->SetLayoutManager(row_layout);
  for (std::size_t i = 1u; i < users.size(); ++i) {
    auto* view = new LoginUserView(LoginDisplayStyle::kExtraSmall,
                                   false /*show_dropdown*/);
    user_views_.push_back(view);
    view->UpdateForUser(users[i]);
    row->AddChildView(view);
  }

  scroller_ = new views::ScrollView();
  scroller_->SetContents(row);
  scroller_->ClipHeightTo(size().height(), size().height());
  AddChildView(scroller_);

  // Padding right of user list.
  AddChildView(MakePreferredSizeView(gfx::Size(
      kHighDensityHorizontalPaddingRightOfUserListDp, kNonEmptyHeightDp)));
}

LockContentsView::UserState* LockContentsView::FindStateForUser(
    const AccountId& user) {
  for (UserState& state : users_) {
    if (state.account_id == user)
      return &state;
  }

  return nullptr;
}

void LockContentsView::UpdateAuthMethodsForAuthUser(bool animate) {
  LockContentsView::UserState* state =
      FindStateForUser(auth_user_view_->current_user());
  DCHECK(state);

  uint32_t auth_methods = LoginAuthUserView::AUTH_NONE;
  if (state->show_pin)
    auth_methods |= LoginAuthUserView::AUTH_PIN;

  // Update to the new layout. Capture existing size so we are able to animate.
  gfx::Rect existing_bounds = auth_user_view_->bounds();
  auth_user_view_->SetAuthMethods(auth_methods);
  Layout();

  if (animate) {
    gfx::Rect new_bounds = auth_user_view_->bounds();
    auth_user_view_->SetBoundsRect(existing_bounds);
    auth_user_view_animator_->AnimateViewTo(auth_user_view_, new_bounds);
  }
}

}  // namespace ash
