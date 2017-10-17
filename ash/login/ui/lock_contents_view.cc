// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view.h"

#include <memory>

#include "ash/focus_cycler.h"
#include "ash/ime/ime_controller.h"
#include "ash/login/lock_screen_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_bubble.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/note_action_launch_button.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

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

constexpr const char kLockContentsViewName[] = "LockContentsView";

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

// Returns the first or last focusable child of |root|. If |reverse| is false,
// this returns the first focusable child. If |reverse| is true, this returns
// the last focusable child.
views::View* FindFirstOrLastFocusableChild(views::View* root, bool reverse) {
  views::FocusSearch search(root, reverse /*cycle*/,
                            false /*accessibility_mode*/);
  views::FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return search.FindNextFocusableView(
      root, reverse, views::FocusSearch::DOWN, false /*check_starting_view*/,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
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

views::View* LockContentsView::TestApi::note_action() const {
  return view_->note_action_;
}

LoginBubble* LockContentsView::TestApi::tooltip_bubble() const {
  return view_->tooltip_bubble_.get();
}

LockContentsView::UserState::UserState(AccountId account_id)
    : account_id(account_id) {}

LockContentsView::UserState::UserState(UserState&&) = default;

LockContentsView::UserState::~UserState() = default;

LockContentsView::LockContentsView(
    mojom::TrayActionState initial_note_action_state,
    LoginDataDispatcher* data_dispatcher)
    : NonAccessibleView(kLockContentsViewName),
      data_dispatcher_(data_dispatcher),
      display_observer_(this) {
  data_dispatcher_->AddObserver(this);
  display_observer_.Add(display::Screen::GetScreen());
  Shell::Get()->lock_screen_controller()->AddLockScreenAppsFocusObserver(this);
  Shell::Get()->system_tray_notifier()->AddSystemTrayFocusObserver(this);
  error_bubble_ = std::make_unique<LoginBubble>();
  tooltip_bubble_ = std::make_unique<LoginBubble>();

  // We reuse the focusable state on this view as a signal that focus should
  // switch to the system tray. LockContentsView should otherwise not be
  // focusable.
  SetFocusBehavior(FocusBehavior::ALWAYS);

  SetLayoutManager(new views::FillLayout());

  main_view_ = new NonAccessibleView();
  AddChildView(main_view_);

  note_action_ =
      new NoteActionLaunchButton(initial_note_action_state, data_dispatcher_);
  AddChildView(note_action_);

  OnLockScreenNoteStateChanged(initial_note_action_state);
}

LockContentsView::~LockContentsView() {
  data_dispatcher_->RemoveObserver(this);
  Shell::Get()->lock_screen_controller()->RemoveLockScreenAppsFocusObserver(
      this);
  Shell::Get()->system_tray_notifier()->RemoveSystemTrayFocusObserver(this);
}

void LockContentsView::Layout() {
  View::Layout();

  // Layout note action in the top right corner - the action origin is offset
  // to the left from the contents view top right corner by the width of the
  // action view.
  note_action_->SizeToPreferredSize();
  gfx::Size action_size = note_action_->GetPreferredSize();
  note_action_->SetPosition(GetLocalBounds().top_right() -
                            gfx::Vector2d(action_size.width(), 0));

  if (scroller_)
    scroller_->ClipHeightTo(size().height(), size().height());
}

void LockContentsView::AddedToWidget() {
  DoLayout();

  // Focus the primary user when showing the UI. This will focus the password.
  if (primary_auth_)
    primary_auth_->RequestFocus();
}

void LockContentsView::OnFocus() {
  // If LockContentsView somehow gains focus (ie, a test, but it should not
  // under typical circumstances), immediately forward the focus to the
  // primary_auth_ since LockContentsView has no real focusable content by
  // itself.
  if (primary_auth_)
    primary_auth_->RequestFocus();
}

void LockContentsView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  // The LockContentsView itself doesn't have anything to focus. If it gets
  // focused we should change the currently focused widget (ie, to the shelf or
  // status area, or lock screen apps, if they are active).
  if (reverse && lock_screen_apps_active_) {
    Shell::Get()->lock_screen_controller()->FocusLockScreenApps(reverse);
    return;
  }

  FocusNextWidget(reverse);
}

void LockContentsView::OnUsersChanged(
    const std::vector<mojom::LoginUserInfoPtr>& users) {
  // The debug view will potentially call this method many times. Make sure to
  // invalidate any child references.
  main_view_->RemoveAllChildViews(true /*delete_children*/);
  user_views_.clear();
  opt_secondary_auth_ = nullptr;
  scroller_ = nullptr;
  rotation_actions_.clear();

  // Build user state list.
  users_.clear();
  for (const mojom::LoginUserInfoPtr& user : users)
    users_.push_back(UserState{user->basic_user_info->account_id});

  main_layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal);
  main_layout_->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  main_layout_->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  main_view_->SetLayoutManager(main_layout_);

  // Add auth user.
  primary_auth_ = AllocateLoginAuthUserView(users[0], true /*is_primary*/);
  main_view_->AddChildView(primary_auth_);

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

  LoginAuthUserView* auth_user =
      TryToFindAuthUser(user, true /*require_auth_active*/);
  if (auth_user)
    LayoutAuth(auth_user, nullptr /*opt_to_hide*/, true /*animate*/);
}

void LockContentsView::OnClickToUnlockEnabledForUserChanged(
    const AccountId& user,
    bool enabled) {
  LockContentsView::UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user enabling click to auth";
    return;
  }
  state->enable_tap_auth = enabled;

  LoginAuthUserView* auth_user =
      TryToFindAuthUser(user, true /*require_auth_active*/);
  if (auth_user)
    LayoutAuth(auth_user, nullptr /*opt_to_hide*/, true /*animate*/);
}

void LockContentsView::OnShowEasyUnlockIcon(
    const AccountId& user,
    const mojom::EasyUnlockIconOptionsPtr& icon) {
  UserState* state = FindStateForUser(user);
  if (!state)
    return;

  state->easy_unlock_state = icon->Clone();
  UpdateEasyUnlockIconForUser(user);

  // Show tooltip only if the user is actively showing auth.
  auto* auth_user = TryToFindAuthUser(user, true /*require_auth_active*/);
  if (auth_user) {
    tooltip_bubble_->Close();
    if (icon->autoshow_tooltip) {
      tooltip_bubble_->ShowTooltip(
          icon->tooltip,
          CurrentAuthUserView()->password_view() /*anchor_view*/);
    }
  }
}

void LockContentsView::OnLockScreenNoteStateChanged(
    mojom::TrayActionState state) {
  bool old_lock_screen_apps_active = lock_screen_apps_active_;
  lock_screen_apps_active_ = state == mojom::TrayActionState::kActive;

  // If lock screen apps just got deactivated - request focus for primary auth,
  // which should focus the password field.
  if (old_lock_screen_apps_active && !lock_screen_apps_active_ && primary_auth_)
    primary_auth_->RequestFocus();
}

void LockContentsView::OnFocusLeavingLockScreenApps(bool reverse) {
  if (!reverse || lock_screen_apps_active_)
    FocusNextWidget(reverse);
  else
    FindFirstOrLastFocusableChild(this, reverse)->RequestFocus();
}

void LockContentsView::OnFocusLeavingSystemTray(bool reverse) {
  // This function is called when the system tray is losing focus. We want to
  // focus the first or last child in this view, or a lock screen app window if
  // one is active (in which case lock contents should not have focus). In the
  // later case, still focus lock screen first, to synchronously take focus away
  // from the system shelf (or tray) - lock shelf view expect the focus to be
  // taken when it passes it to lock screen view, and can misbehave in case the
  // focus is kept in it.
  FindFirstOrLastFocusableChild(this, reverse)->RequestFocus();

  if (lock_screen_apps_active_) {
    Shell::Get()->lock_screen_controller()->FocusLockScreenApps(reverse);
    return;
  }
}

void LockContentsView::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t changed_metrics) {
  // Ignore all metric changes except rotation.
  if ((changed_metrics & DISPLAY_METRIC_ROTATION) == 0)
    return;

  DoLayout();
}

void LockContentsView::FocusNextWidget(bool reverse) {
  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  // Tell the focus direction to the status area or the shelf so they can focus
  // the correct child view.
  if (reverse) {
    shelf->GetStatusAreaWidget()
        ->status_area_widget_delegate()
        ->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->GetStatusAreaWidget());
  } else {
    shelf->shelf_widget()->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
  }
}

void LockContentsView::CreateLowDensityLayout(
    const std::vector<mojom::LoginUserInfoPtr>& users) {
  DCHECK_EQ(users.size(), 2u);

  // Space between auth user and alternative user.
  main_view_->AddChildView(MakeOrientationViewWithWidths(
      kLowDensityDistanceBetweenUsersInLandscapeDp,
      kLowDensityDistanceBetweenUsersInPortraitDp));

  // Build auth user.
  opt_secondary_auth_ =
      AllocateLoginAuthUserView(users[1], false /*is_primary*/);
  opt_secondary_auth_->SetAuthMethods(LoginAuthUserView::AUTH_NONE);
  main_view_->AddChildView(opt_secondary_auth_);
}

void LockContentsView::CreateMediumDensityLayout(
    const std::vector<mojom::LoginUserInfoPtr>& users) {
  // Insert spacing before (left of) auth.
  main_view_->AddChildViewAt(MakeOrientationViewWithWidths(
                                 kMediumDensityMarginLeftOfAuthUserLandscapeDp,
                                 kMediumDensityMarginLeftOfAuthUserPortraitDp),
                             0);
  // Insert spacing between auth and user list.
  main_view_->AddChildView(MakeOrientationViewWithWidths(
      kMediumDensityDistanceBetweenAuthUserAndUsersLandscapeDp,
      kMediumDensityDistanceBetweenAuthUserAndUsersPortraitDp));

  // Add additional users.
  auto* row = new NonAccessibleView();
  main_view_->AddChildView(row);
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
  auto* left = new NonAccessibleView();
  main_view_->AddChildViewAt(left, 0);
  auto* right = new NonAccessibleView();
  main_view_->AddChildView(right);
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
      main_layout_, left, right));
}

void LockContentsView::CreateHighDensityLayout(
    const std::vector<mojom::LoginUserInfoPtr>& users) {
  // TODO: Finish 7+ user layout.

  // Insert spacing before and after the auth view.
  auto* fill = new NonAccessibleView();
  main_view_->AddChildViewAt(fill, 0);
  main_layout_->SetFlexForView(fill, 1);

  fill = new NonAccessibleView();
  main_view_->AddChildView(fill);
  main_layout_->SetFlexForView(fill, 1);

  // Padding left of user list.
  main_view_->AddChildView(MakeOrientationViewWithWidths(
      kHighDensityHorizontalPaddingLeftOfUserListLandscapeDp,
      kHighDensityHorizontalPaddingLeftOfUserListPortraitDp));

  // Add user list.
  auto* row = new NonAccessibleView();
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
  main_view_->AddChildView(scroller_);

  // Padding right of user list.
  main_view_->AddChildView(MakeOrientationViewWithWidths(
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

void LockContentsView::SwapActiveAuthBetweenPrimaryAndSecondary(
    bool is_primary) {
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
  if (auth_success) {
    unlock_attempt_ = 0;
    error_bubble_->Close();
  } else {
    ShowErrorMessage();
    ++unlock_attempt_;
  }
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
  UserState* state =
      FindStateForUser(to_update->current_user()->basic_user_info->account_id);
  if (state->show_pin)
    to_update_auth |= LoginAuthUserView::AUTH_PIN;
  if (state->enable_tap_auth)
    to_update_auth |= LoginAuthUserView::AUTH_TAP;
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
  mojom::LoginUserInfoPtr previous_auth_user =
      primary_auth_->current_user()->Clone();
  mojom::LoginUserInfoPtr new_auth_user = view->current_user()->Clone();

  view->UpdateForUser(previous_auth_user, true /*animate*/);
  primary_auth_->UpdateForUser(new_auth_user);
  LayoutAuth(primary_auth_, nullptr, true /*animate*/);
  OnAuthUserChanged();
}

void LockContentsView::OnAuthUserChanged() {
  const AccountId new_auth_user =
      CurrentAuthUserView()->current_user()->basic_user_info->account_id;

  Shell::Get()->lock_screen_controller()->OnFocusPod(new_auth_user);
  UpdateEasyUnlockIconForUser(new_auth_user);

  // Reset unlock attempt when the auth user changes.
  unlock_attempt_ = 0;
}

void LockContentsView::UpdateEasyUnlockIconForUser(const AccountId& user) {
  // Try to find an auth view for |user|. If there is none, there is no state to
  // update.
  LoginAuthUserView* auth_view =
      TryToFindAuthUser(user, false /*require_auth_active*/);
  if (!auth_view)
    return;

  UserState* state = FindStateForUser(user);
  DCHECK(state);

  // Hide easy unlock icon if there is no data is available.
  if (!state->easy_unlock_state) {
    auth_view->SetEasyUnlockIcon(mojom::EasyUnlockIconId::NONE,
                                 base::string16());
    return;
  }

  // TODO(jdufault): Make easy unlock backend always send aria_label, right now
  // it is only sent if there is no tooltip.
  base::string16 accessibility_label = state->easy_unlock_state->aria_label;
  if (accessibility_label.empty())
    accessibility_label = state->easy_unlock_state->tooltip;

  auth_view->SetEasyUnlockIcon(state->easy_unlock_state->icon,
                               accessibility_label);
}

LoginAuthUserView* LockContentsView::CurrentAuthUserView() {
  if (opt_secondary_auth_ &&
      opt_secondary_auth_->auth_methods() != LoginAuthUserView::AUTH_NONE) {
    DCHECK(primary_auth_->auth_methods() == LoginAuthUserView::AUTH_NONE);
    return opt_secondary_auth_;
  }

  return primary_auth_;
}

void LockContentsView::ShowErrorMessage() {
  std::string error_text = l10n_util::GetStringUTF8(
      unlock_attempt_ ? IDS_ASH_LOGIN_ERROR_AUTHENTICATING_2ND_TIME
                      : IDS_ASH_LOGIN_ERROR_AUTHENTICATING);
  ImeController* ime_controller = Shell::Get()->ime_controller();
  if (ime_controller->IsCapsLockEnabled()) {
    error_text +=
        "\n" + l10n_util::GetStringUTF8(IDS_ASH_LOGIN_ERROR_CAPS_LOCK_HINT);
  }

  // Display a hint to switch keyboards if there are other active input
  // methods.
  if (ime_controller->available_imes().size() > 1) {
    error_text += "\n" + l10n_util::GetStringUTF8(
                             IDS_ASH_LOGIN_ERROR_KEYBOARD_SWITCH_HINT);
  }

  error_bubble_->ShowErrorBubble(
      base::UTF8ToUTF16(error_text),
      CurrentAuthUserView()->password_view() /*anchor_view*/);
}

void LockContentsView::OnEasyUnlockIconHovered() {
  UserState* state = FindStateForUser(
      CurrentAuthUserView()->current_user()->basic_user_info->account_id);
  DCHECK(state);
  mojom::EasyUnlockIconOptionsPtr& easy_unlock_state = state->easy_unlock_state;
  DCHECK(easy_unlock_state);

  if (!easy_unlock_state->tooltip.empty()) {
    tooltip_bubble_->ShowTooltip(
        easy_unlock_state->tooltip,
        CurrentAuthUserView()->password_view() /*anchor_view*/);
  }
}

void LockContentsView::OnEasyUnlockIconTapped() {
  UserState* state = FindStateForUser(
      CurrentAuthUserView()->current_user()->basic_user_info->account_id);
  DCHECK(state);
  mojom::EasyUnlockIconOptionsPtr& easy_unlock_state = state->easy_unlock_state;
  DCHECK(easy_unlock_state);

  if (easy_unlock_state->hardlock_on_click) {
    AccountId user =
        CurrentAuthUserView()->current_user()->basic_user_info->account_id;
    Shell::Get()->lock_screen_controller()->HardlockPod(user);
    // TODO(jdufault): This should get called as a result of HardlockPod.
    OnClickToUnlockEnabledForUserChanged(user, false /*enabled*/);
  }
}

LoginAuthUserView* LockContentsView::AllocateLoginAuthUserView(
    const mojom::LoginUserInfoPtr& user,
    bool is_primary) {
  return new LoginAuthUserView(
      user,
      base::Bind(&LockContentsView::OnAuthenticate, base::Unretained(this)),
      base::Bind(&LockContentsView::SwapActiveAuthBetweenPrimaryAndSecondary,
                 base::Unretained(this), is_primary),
      base::Bind(&LockContentsView::OnEasyUnlockIconHovered,
                 base::Unretained(this)),
      base::Bind(&LockContentsView::OnEasyUnlockIconTapped,
                 base::Unretained(this)));
}

LoginAuthUserView* LockContentsView::TryToFindAuthUser(
    const AccountId& user,
    bool require_auth_active) {
  LoginAuthUserView* view = nullptr;

  // Find auth instance.
  if (primary_auth_->current_user()->basic_user_info->account_id == user) {
    view = primary_auth_;
  } else if (opt_secondary_auth_ &&
             opt_secondary_auth_->current_user()->basic_user_info->account_id ==
                 user) {
    view = opt_secondary_auth_;
  }

  // Make sure auth instance is active if required.
  if (require_auth_active && view &&
      view->auth_methods() == LoginAuthUserView::AUTH_NONE) {
    view = nullptr;
  }

  return view;
};

}  // namespace ash
