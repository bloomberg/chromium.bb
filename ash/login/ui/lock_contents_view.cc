// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view.h"

#include "ash/login/lock_screen_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/shell.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Any non-zero value used for separator height. Makes debugging easier; this
// should not affect visual appearance.
constexpr int kNonEmptyHeightDp = 30;

// Horizontal distance between two users in the low density layout.
constexpr int kLowDensityDistanceBetweenUsersInLandscapeDp = 118;
constexpr int kLowDensityDistanceBetweenUsersInPortraitDp = 32;

// Margin left of the auth user in the medium density layout.
constexpr int kMediumDensityMarginLeftOfAuthUserLandscapeDp = 98;
constexpr int kMediumDensityMarginLeftOfAuthUserPortraitDp = 0;

// Horizontal distance between the auth user and the medium density user row.
constexpr int kMediumDensityDistanceBetweenAuthUserAndUsersLandscapeDp = 220;
constexpr int kMediumDensityDistanceBetweenAuthUserAndUsersPortraitDp = 84;

// Vertical padding between each entry in the medium density user row
constexpr int kMediumDensityVerticalDistanceBetweenUsersDp = 53;

// Horizontal padding left and right of the high density user list.
constexpr int kHighDensityHorizontalPaddingLeftOfUserListLandscapeDp = 72;
constexpr int kHighDensityHorizontalPaddingRightOfUserListLandscapeDp = 72;
constexpr int kHighDensityHorizontalPaddingLeftOfUserListPortraitDp = 46;
constexpr int kHighDensityHorizontalPaddingRightOfUserListPortraitDp = 12;

// The vertical padding between each entry in the extra-small user row
constexpr int kHighDensityVerticalDistanceBetweenUsersDp = 32;

// A view which stores two preferred sizes. The embedder can control which one
// is used.
class MultiSizedView : public views::View {
 public:
  MultiSizedView(const gfx::Size& a, const gfx::Size& b) : a_(a), b_(b) {}
  ~MultiSizedView() override = default;

  void SwapPreferredSizeTo(bool use_a) {
    if (use_a)
      SetPreferredSize(a_);
    else
      SetPreferredSize(b_);
  }

 private:
  gfx::Size a_;
  gfx::Size b_;

  DISALLOW_COPY_AND_ASSIGN(MultiSizedView);
};

// Returns true if landscape constants should be used for UI shown in |widget|.
bool ShouldShowLandscape(views::Widget* widget) {
  // |widget| is null when the view is being constructed. Default to landscape
  // in that case. A new layout will happen when the view is attached to a
  // widget (see LockContentsView::AddedToWidget), which will let us fetch the
  // correct display orientation.
  if (!widget)
    return true;

  // Get the orientation for |widget|.
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());
  display::ManagedDisplayInfo info =
      Shell::Get()->display_manager()->GetDisplayInfo(display.id());

  // Return true if it is landscape.
  switch (info.GetActiveRotation()) {
    case display::Display::ROTATE_0:
    case display::Display::ROTATE_180:
      return true;
    case display::Display::ROTATE_90:
    case display::Display::ROTATE_270:
      return false;
  }
  NOTREACHED();
  return true;
}

}  // namespace

LockContentsView::TestApi::TestApi(LockContentsView* view) : view_(view) {}

LockContentsView::TestApi::~TestApi() = default;

LoginAuthUserView* LockContentsView::TestApi::primary_auth() const {
  return view_->primary_auth_;
}

LoginAuthUserView* LockContentsView::TestApi::opt_secondary_auth() const {
  return view_->opt_secondary_auth_;
}

const std::vector<LoginUserView*>& LockContentsView::TestApi::user_views()
    const {
  return view_->user_views_;
}

LockContentsView::UserState::UserState(AccountId account_id)
    : account_id(account_id) {}

LockContentsView::LockContentsView(LoginDataDispatcher* data_dispatcher)
    : data_dispatcher_(data_dispatcher), display_observer_(this) {
  data_dispatcher_->AddObserver(this);
  display_observer_.Add(display::Screen::GetScreen());
}

LockContentsView::~LockContentsView() {
  data_dispatcher_->RemoveObserver(this);
}

void LockContentsView::Layout() {
  View::Layout();
  if (scroller_)
    scroller_->ClipHeightTo(size().height(), size().height());
}

void LockContentsView::AddedToWidget() {
  DoLayout();

  // Focus the primary user when showing the UI. This will focus the password.
  primary_auth_->RequestFocus();
}

void LockContentsView::OnUsersChanged(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  // The debug view will potentially call this method many times. Make sure to
  // invalidate any child references.
  RemoveAllChildViews(true /*delete_children*/);
  user_views_.clear();
  opt_secondary_auth_ = nullptr;
  scroller_ = nullptr;
  root_layout_ = nullptr;
  rotation_actions_.clear();

  // Build user state list.
  users_.clear();
  for (const ash::mojom::UserInfoPtr& user : users)
    users_.push_back(UserState{user->account_id});

  // Build view hierarchy.
  root_layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal);
  root_layout_->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  root_layout_->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(root_layout_);

  // Add auth user.
  primary_auth_ = new LoginAuthUserView(
      users[0],
      base::Bind(&LockContentsView::OnAuthenticate, base::Unretained(this)),
      base::Bind(&LockContentsView::SwapPrimaryAndSecondaryAuth,
                 base::Unretained(this), true /*is_primary*/));
  AddChildView(primary_auth_);

  // Build layout for additional users.
  if (users.size() == 2)
    CreateLowDensityLayout(users);
  else if (users.size() >= 3 && users.size() <= 6)
    CreateMediumDensityLayout(users);
  else if (users.size() >= 7)
    CreateHighDensityLayout(users);

  LayoutAuth(primary_auth_, opt_secondary_auth_, false /*animate*/);

  // Auth user may be the same if we already built lock screen.
  OnAuthUserChanged();

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

  // We need to update the auth display if |user| is currently shown in either
  // |primary_auth_| or |opt_secondary_auth_|.
  if (primary_auth_->current_user()->account_id == state->account_id &&
      primary_auth_->auth_methods() != LoginAuthUserView::AUTH_NONE) {
    LayoutAuth(primary_auth_, nullptr, true /*animate*/);
  } else if (opt_secondary_auth_ &&
             opt_secondary_auth_->current_user()->account_id ==
                 state->account_id &&
             opt_secondary_auth_->auth_methods() !=
                 LoginAuthUserView::AUTH_NONE) {
    LayoutAuth(opt_secondary_auth_, nullptr, true /*animate*/);
  }
}

void LockContentsView::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t changed_metrics) {
  // Ignore all metric changes except rotation.
  if ((changed_metrics & DISPLAY_METRIC_ROTATION) == 0)
    return;

  DoLayout();
}

void LockContentsView::CreateLowDensityLayout(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  DCHECK_EQ(users.size(), 2u);

  // Space between auth user and alternative user.
  AddChildView(MakeOrientationViewWithWidths(
      kLowDensityDistanceBetweenUsersInLandscapeDp,
      kLowDensityDistanceBetweenUsersInPortraitDp));

  // Build auth user.
  opt_secondary_auth_ = new LoginAuthUserView(
      users[1],
      base::Bind(&LockContentsView::OnAuthenticate, base::Unretained(this)),
      base::Bind(&LockContentsView::SwapPrimaryAndSecondaryAuth,
                 base::Unretained(this), false /*is_primary*/));
  opt_secondary_auth_->SetAuthMethods(LoginAuthUserView::AUTH_NONE);
  AddChildView(opt_secondary_auth_);
}

void LockContentsView::CreateMediumDensityLayout(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  // Insert spacing before (left of) auth.
  AddChildViewAt(MakeOrientationViewWithWidths(
                     kMediumDensityMarginLeftOfAuthUserLandscapeDp,
                     kMediumDensityMarginLeftOfAuthUserPortraitDp),
                 0);
  // Insert spacing between auth and user list.
  AddChildView(MakeOrientationViewWithWidths(
      kMediumDensityDistanceBetweenAuthUserAndUsersLandscapeDp,
      kMediumDensityDistanceBetweenAuthUserAndUsersPortraitDp));

  // Add additional users.
  auto* row = new views::View();
  AddChildView(row);
  auto* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(),
                           kMediumDensityVerticalDistanceBetweenUsersDp);
  row->SetLayoutManager(layout);
  for (std::size_t i = 1u; i < users.size(); ++i) {
    auto* view =
        new LoginUserView(LoginDisplayStyle::kSmall, false /*show_dropdown*/,
                          base::Bind(&LockContentsView::SwapToAuthUser,
                                     base::Unretained(this), i - 1) /*on_tap*/);
    user_views_.push_back(view);
    view->UpdateForUser(users[i], false /*animate*/);
    row->AddChildView(view);
  }

  // Insert dynamic spacing on left/right of the content which changes based on
  // screen rotation and display size.
  auto* left = new views::View();
  AddChildViewAt(left, 0);
  auto* right = new views::View();
  AddChildView(right);
  AddRotationAction(base::BindRepeating(
      [](views::BoxLayout* layout, views::View* left, views::View* right,
         bool landscape) {
        if (landscape) {
          layout->SetFlexForView(left, 1);
          layout->SetFlexForView(right, 1);
        } else {
          layout->SetFlexForView(left, 2);
          layout->SetFlexForView(right, 1);
        }
      },
      root_layout_, left, right));
}

void LockContentsView::CreateHighDensityLayout(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  // TODO: Finish 7+ user layout.

  // Insert spacing before and after the auth view.
  auto* fill = new views::View();
  AddChildViewAt(fill, 0);
  root_layout_->SetFlexForView(fill, 1);

  fill = new views::View();
  AddChildView(fill);
  root_layout_->SetFlexForView(fill, 1);

  // Padding left of user list.
  AddChildView(MakeOrientationViewWithWidths(
      kHighDensityHorizontalPaddingLeftOfUserListLandscapeDp,
      kHighDensityHorizontalPaddingLeftOfUserListPortraitDp));

  // Add user list.
  auto* row = new views::View();
  auto* row_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(),
                           kHighDensityVerticalDistanceBetweenUsersDp);
  row_layout->set_minimum_cross_axis_size(
      LoginUserView::WidthForLayoutStyle(LoginDisplayStyle::kExtraSmall));
  row->SetLayoutManager(row_layout);
  for (std::size_t i = 1u; i < users.size(); ++i) {
    auto* view = new LoginUserView(
        LoginDisplayStyle::kExtraSmall, false /*show_dropdown*/,
        base::Bind(&LockContentsView::SwapToAuthUser, base::Unretained(this),
                   i - 1) /*on_tap*/);
    user_views_.push_back(view);
    view->UpdateForUser(users[i], false /*animate*/);
    row->AddChildView(view);
  }
  scroller_ = new views::ScrollView();
  scroller_->SetContents(row);
  scroller_->ClipHeightTo(size().height(), size().height());
  AddChildView(scroller_);

  // Padding right of user list.
  AddChildView(MakeOrientationViewWithWidths(
      kHighDensityHorizontalPaddingRightOfUserListLandscapeDp,
      kHighDensityHorizontalPaddingRightOfUserListPortraitDp));
}

void LockContentsView::DoLayout() {
  bool landscape = ShouldShowLandscape(GetWidget());
  for (auto& action : rotation_actions_)
    action.Run(landscape);

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          GetWidget()->GetNativeWindow());
  SetPreferredSize(display.size());
  SizeToPreferredSize();
  Layout();
}

views::View* LockContentsView::MakeOrientationViewWithWidths(int landscape,
                                                             int portrait) {
  auto* view = new MultiSizedView(gfx::Size(landscape, kNonEmptyHeightDp),
                                  gfx::Size(portrait, kNonEmptyHeightDp));
  AddRotationAction(base::BindRepeating(&MultiSizedView::SwapPreferredSizeTo,
                                        base::Unretained(view)));
  return view;
}

void LockContentsView::AddRotationAction(const OnRotate& on_rotate) {
  on_rotate.Run(ShouldShowLandscape(GetWidget()));
  rotation_actions_.push_back(on_rotate);
}

void LockContentsView::SwapPrimaryAndSecondaryAuth(bool is_primary) {
  if (is_primary &&
      primary_auth_->auth_methods() == LoginAuthUserView::AUTH_NONE) {
    LayoutAuth(primary_auth_, opt_secondary_auth_, true /*animate*/);
    OnAuthUserChanged();
  } else if (!is_primary && opt_secondary_auth_ &&
             opt_secondary_auth_->auth_methods() ==
                 LoginAuthUserView::AUTH_NONE) {
    LayoutAuth(opt_secondary_auth_, primary_auth_, true /*animate*/);
    OnAuthUserChanged();
  }
}

void LockContentsView::OnAuthenticate(bool auth_success) {
  if (auth_success)
    ash::LockScreen::Get()->Destroy();
}

LockContentsView::UserState* LockContentsView::FindStateForUser(
    const AccountId& user) {
  for (UserState& state : users_) {
    if (state.account_id == user)
      return &state;
  }

  return nullptr;
}

void LockContentsView::LayoutAuth(LoginAuthUserView* to_update,
                                  LoginAuthUserView* opt_to_hide,
                                  bool animate) {
  // Capture animation metadata before we changing state.
  if (animate) {
    to_update->CaptureStateForAnimationPreLayout();
    if (opt_to_hide)
      opt_to_hide->CaptureStateForAnimationPreLayout();
  }

  // Update auth methods for |to_update|. Disable auth on |opt_to_hide|.
  uint32_t to_update_auth = LoginAuthUserView::AUTH_PASSWORD;
  if (FindStateForUser(to_update->current_user()->account_id)->show_pin)
    to_update_auth |= LoginAuthUserView::AUTH_PIN;
  to_update->SetAuthMethods(to_update_auth);
  if (opt_to_hide)
    opt_to_hide->SetAuthMethods(LoginAuthUserView::AUTH_NONE);

  Layout();

  // Apply animations.
  if (animate) {
    to_update->ApplyAnimationPostLayout();
    if (opt_to_hide)
      opt_to_hide->ApplyAnimationPostLayout();
  }
}

void LockContentsView::SwapToAuthUser(int user_index) {
  auto* view = user_views_[user_index];
  mojom::UserInfoPtr previous_auth_user =
      primary_auth_->current_user()->Clone();
  mojom::UserInfoPtr new_auth_user = view->current_user()->Clone();

  view->UpdateForUser(previous_auth_user, true /*animate*/);
  primary_auth_->UpdateForUser(new_auth_user);
  LayoutAuth(primary_auth_, nullptr, true /*animate*/);
  OnAuthUserChanged();
}

void LockContentsView::OnAuthUserChanged() {
  Shell::Get()->lock_screen_controller()->OnFocusPod(
      CurrentAuthUserView()->current_user()->account_id);
}

LoginAuthUserView* LockContentsView::CurrentAuthUserView() {
  if (opt_secondary_auth_ &&
      opt_secondary_auth_->auth_methods() != LoginAuthUserView::AUTH_NONE) {
    DCHECK(primary_auth_->auth_methods() == LoginAuthUserView::AUTH_NONE);
    return opt_secondary_auth_;
  }

  return primary_auth_;
}

}  // namespace ash
