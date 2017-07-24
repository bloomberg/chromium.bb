// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view.h"

#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/shell.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/bounds_animator.h"
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

// Duration (in milliseconds) of the auth user view animation, ie, when enabling
// or disabling the PIN keyboard.
constexpr int kAuthUserViewAnimationDurationMs = 100;

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
  background_->SetSize(size());
}

void LockContentsView::AddedToWidget() {
  DoLayout();
}

void LockContentsView::OnUsersChanged(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  // The debug view will potentially call this method many times. Make sure to
  // invalidate any child references.
  RemoveAllChildViews(true /*delete_children*/);
  user_views_.clear();
  scroller_ = nullptr;
  root_layout_ = nullptr;
  rotation_actions_.clear();

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

  root_layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal);
  root_layout_->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  root_layout_->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(root_layout_);

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
    CreateHighDensityLayout(users);

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

void LockContentsView::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t changed_metrics) {
  // Ignore all metric changes except rotation.
  if ((changed_metrics & DISPLAY_METRIC_ROTATION) == 0)
    return;

  DoLayout();
}

void LockContentsView::CreateLowDensityLayout(
    const std::vector<ash::mojom::UserInfoPtr>& users) {
  // Space between auth user and alternative user.
  AddChildView(MakeOrientationViewWithWidths(
      kLowDensityDistanceBetweenUsersInLandscapeDp,
      kLowDensityDistanceBetweenUsersInPortraitDp));
  // TODO(jdufault): When alt_user_view is clicked we should show auth methods.
  auto* alt_user_view =
      new LoginUserView(LoginDisplayStyle::kLarge, false /*show_dropdown*/);
  alt_user_view->UpdateForUser(users[1]);
  user_views_.push_back(alt_user_view);
  AddChildView(alt_user_view);
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
        new LoginUserView(LoginDisplayStyle::kSmall, false /*show_dropdown*/);
    user_views_.push_back(view);
    view->UpdateForUser(users[i]);
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
