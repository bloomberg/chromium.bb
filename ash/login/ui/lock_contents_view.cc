// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view.h"

#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_user_view.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Any non-zero value used for separator height. Makes debugging easier.
constexpr int kNonEmptyHeightDp = 30;

// Horizontal distance between the two users in the two-user layout.
constexpr int kDistanceBetweenUsersInTwoUserModeDp = 118;

// Margin left of the auth user in the small layout.
constexpr int kSmallMarginLeftOfAuthUserDp = 98;
// The horizontal distance between the auth user and the small user row.
constexpr int kSmallDistanceBetweenAuthUserAndUsersDp = 220;
// The vertical padding between each entry in the small user row
constexpr int kSmallVerticalDistanceBetweenUsersDp = 53;
// The horizontal padding left of and right of the extra-small user list.
constexpr int kExtraSmallHorizontalPaddingLeftOfRightOfUserListDp = 72;
// The vertical padding between each entry in the extra-small user row
constexpr int kExtraSmallVerticalDistanceBetweenUsersDp = 32;

// Builds a view with the given preferred size.
views::View* MakePreferredSizeView(gfx::Size size) {
  auto* view = new views::View();
  view->SetPreferredSize(size);
  return view;
}

// Wraps |view| in a horizontal box layout. This lets the view get the correct
// preferred vertical size.
views::View* WrapViewForPreferredVerticalSize(views::View* view) {
  auto* host = new views::View();
  host->SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal));
  host->AddChildView(view);
  return host;
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
  UpdateAuthMethodsForAuthUser();
  AddChildView(WrapViewForPreferredVerticalSize(auth_user_view_));

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
  UpdateAuthMethodsForAuthUser();
}

void LockContentsView::CreateLowDensityLayout(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  // Space between auth user and alternative user.
  AddChildView(MakePreferredSizeView(
      gfx::Size(kDistanceBetweenUsersInTwoUserModeDp, 30)));
  auto* alt_user_view =
      new LoginUserView(LoginDisplayStyle::kLarge, false /*show_dropdown*/);
  alt_user_view->UpdateForUser(users[1]);
  user_views_.push_back(alt_user_view);
  AddChildView(WrapViewForPreferredVerticalSize(alt_user_view));
}

void LockContentsView::CreateMediumDensityLayout(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  // Insert spacing before auth and also between the auth and user list.
  AddChildViewAt(MakePreferredSizeView(gfx::Size(kSmallMarginLeftOfAuthUserDp,
                                                 kNonEmptyHeightDp)),
                 0);
  AddChildView(MakePreferredSizeView(
      gfx::Size(kSmallDistanceBetweenAuthUserAndUsersDp, kNonEmptyHeightDp)));

  // Add additional users.
  auto* row = new views::View();
  AddChildView(row);
  auto* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(),
                           kSmallVerticalDistanceBetweenUsersDp);
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

  // Add additional users.
  auto* row = new views::View();
  auto* row_layout = new views::BoxLayout(
      views::BoxLayout::kVertical,
      gfx::Insets(kExtraSmallHorizontalPaddingLeftOfRightOfUserListDp, 0),
      kExtraSmallVerticalDistanceBetweenUsersDp);
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
}

LockContentsView::UserState* LockContentsView::FindStateForUser(
    const AccountId& user) {
  for (UserState& state : users_) {
    if (state.account_id == user)
      return &state;
  }

  return nullptr;
}

void LockContentsView::UpdateAuthMethodsForAuthUser() {
  LockContentsView::UserState* state =
      FindStateForUser(auth_user_view_->current_user());
  DCHECK(state);

  uint32_t auth_methods = LoginAuthUserView::AUTH_NONE;
  if (state->show_pin)
    auth_methods |= LoginAuthUserView::AUTH_PIN;

  auth_user_view_->SetAuthMethods(auth_methods);
  Layout();
}

}  // namespace ash
