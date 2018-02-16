// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_user_view.h"

#include <memory>

#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/layout_util.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/pin_keyboard_animation.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr const char kLoginAuthUserViewClassName[] = "LoginAuthUserView";

// Distance between the user view (ie, the icon and name) and the password
// textfield.
const int kDistanceBetweenUserViewAndPasswordDp = 28;

// Distance between the password textfield and the the pin keyboard.
const int kDistanceBetweenPasswordFieldAndPinKeyboard = 20;

// Distance from the end of pin keyboard to the bottom of the big user view.
const int kDistanceFromPinKeyboardToBigUserViewBottom = 48;

// Returns an observer that will hide |view| when it fires. The observer will
// delete itself after firing. Make sure to call |observer->SetReady()| after
// attaching it.
ui::CallbackLayerAnimationObserver* BuildObserverToHideView(views::View* view) {
  return new ui::CallbackLayerAnimationObserver(base::Bind(
      [](views::View* view,
         const ui::CallbackLayerAnimationObserver& observer) {
        // Don't hide the view if the animation is aborted, as |view| may no
        // longer be valid.
        if (observer.aborted_count())
          return true;

        view->SetVisible(false);
        return true;
      },
      view));
}

}  // namespace

struct LoginAuthUserView::AnimationState {
  int non_pin_y_start_in_screen = 0;
  gfx::Point pin_start_in_screen;
  bool had_pin = false;
  bool had_password = false;

  explicit AnimationState(LoginAuthUserView* view) {
    non_pin_y_start_in_screen = view->GetBoundsInScreen().y();
    pin_start_in_screen = view->pin_view_->GetBoundsInScreen().origin();

    had_pin = (view->auth_methods() & LoginAuthUserView::AUTH_PIN) != 0;
    had_password =
        (view->auth_methods() & LoginAuthUserView::AUTH_PASSWORD) != 0;
  }
};

LoginAuthUserView::TestApi::TestApi(LoginAuthUserView* view) : view_(view) {}

LoginAuthUserView::TestApi::~TestApi() = default;

LoginUserView* LoginAuthUserView::TestApi::user_view() const {
  return view_->user_view_;
}

LoginPasswordView* LoginAuthUserView::TestApi::password_view() const {
  return view_->password_view_;
}

LoginPinView* LoginAuthUserView::TestApi::pin_view() const {
  return view_->pin_view_;
}

LoginAuthUserView::LoginAuthUserView(
    const mojom::LoginUserInfoPtr& user,
    const OnAuthCallback& on_auth,
    const LoginUserView::OnTap& on_tap,
    const OnEasyUnlockIconHovered& on_easy_unlock_icon_hovered,
    const OnEasyUnlockIconTapped& on_easy_unlock_icon_tapped)
    : NonAccessibleView(kLoginAuthUserViewClassName),
      on_auth_(on_auth),
      on_tap_(on_tap),
      weak_factory_(this) {
  // Build child views.
  user_view_ = new LoginUserView(
      LoginDisplayStyle::kLarge, true /*show_dropdown*/,
      base::Bind(&LoginAuthUserView::OnUserViewTap, base::Unretained(this)));

  password_view_ = new LoginPasswordView();
  password_view_->SetPaintToLayer();  // Needed for opacity animation.
  password_view_->layer()->SetFillsBoundsOpaquely(false);
  password_view_->UpdateForUser(user);

  pin_view_ =
      new LoginPinView(base::BindRepeating(&LoginPasswordView::InsertNumber,
                                           base::Unretained(password_view_)),
                       base::BindRepeating(&LoginPasswordView::Backspace,
                                           base::Unretained(password_view_)));
  DCHECK(pin_view_->layer());

  // Initialization of |password_view_| is deferred because it needs the
  // |pin_view_| pointer.
  password_view_->Init(
      base::Bind(&LoginAuthUserView::OnAuthSubmit, base::Unretained(this)),
      base::Bind(&LoginPinView::OnPasswordTextChanged,
                 base::Unretained(pin_view_)),
      on_easy_unlock_icon_hovered, on_easy_unlock_icon_tapped);

  // Child views animate outside view bounds.
  SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);

  // Build layout.
  auto* wrapped_user_view =
      login_layout_util::WrapViewForPreferredSize(user_view_);
  auto* wrapped_pin_view =
      login_layout_util::WrapViewForPreferredSize(pin_view_);

  // Add views in tabbing order; they are rendered in a different order below.
  AddChildView(password_view_);
  AddChildView(wrapped_pin_view);
  AddChildView(wrapped_user_view);

  // Use views::GridLayout instead of views::BoxLayout because views::BoxLayout
  // lays out children according to the view->children order.
  views::GridLayout* grid_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  views::ColumnSet* column_set = grid_layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                        0 /*resize_percent*/, views::GridLayout::USE_PREF,
                        0 /*fixed_width*/, 0 /*min_width*/);
  auto add_view = [&](views::View* view) {
    grid_layout->StartRow(0 /*vertical_resize*/, 0 /*column_set_id*/);
    grid_layout->AddView(view);
  };
  auto add_padding = [&](int amount) {
    grid_layout->AddPaddingRow(0 /*vertical_resize*/, amount /*size*/);
  };

  // Add views in rendering order.
  add_view(wrapped_user_view);
  add_padding(kDistanceBetweenUserViewAndPasswordDp);
  add_view(password_view_);
  add_padding(kDistanceBetweenPasswordFieldAndPinKeyboard);
  add_view(wrapped_pin_view);
  add_padding(kDistanceFromPinKeyboardToBigUserViewBottom);

  // Update authentication UI.
  SetAuthMethods(auth_methods_);
  user_view_->UpdateForUser(user, false /*animate*/);
}

LoginAuthUserView::~LoginAuthUserView() = default;

void LoginAuthUserView::SetAuthMethods(uint32_t auth_methods) {
  auth_methods_ = static_cast<AuthMethods>(auth_methods);
  bool has_password = HasAuthMethod(AUTH_PASSWORD);
  bool has_pin = HasAuthMethod(AUTH_PIN);
  bool has_tap = HasAuthMethod(AUTH_TAP);

  password_view_->SetEnabled(has_password);
  password_view_->SetFocusEnabledForChildViews(has_password);
  password_view_->layer()->SetOpacity(has_password ? 1 : 0);

  if (has_password)
    password_view_->RequestFocus();

  pin_view_->SetVisible(has_pin);

  // Note: if both |has_tap| and |has_pin| are true, prefer tap placeholder.
  if (has_tap) {
    password_view_->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_TAP_PLACEHOLDER));
  } else if (has_pin) {
    password_view_->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PIN_PLACEHOLDER));
  } else {
    password_view_->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PLACEHOLDER));
  }

  // Only the active auth user view has a password displayed. If that is the
  // case, then render the user view as if it was always focused, since clicking
  // on it will not do anything (such as swapping users).
  user_view_->SetForceOpaque(has_password);
  user_view_->SetTapEnabled(!has_password);

  PreferredSizeChanged();
}

void LoginAuthUserView::SetEasyUnlockIcon(
    mojom::EasyUnlockIconId id,
    const base::string16& accessibility_label) {
  password_view_->SetEasyUnlockIcon(id, accessibility_label);
}

void LoginAuthUserView::CaptureStateForAnimationPreLayout() {
  DCHECK(!cached_animation_state_);
  cached_animation_state_ = std::make_unique<AnimationState>(this);
}

void LoginAuthUserView::ApplyAnimationPostLayout() {
  DCHECK(cached_animation_state_);

  // Cancel any running animations.
  pin_view_->layer()->GetAnimator()->AbortAllAnimations();
  password_view_->layer()->GetAnimator()->AbortAllAnimations();
  layer()->GetAnimator()->AbortAllAnimations();

  bool has_password = (auth_methods() & AUTH_PASSWORD) != 0;
  bool has_pin = (auth_methods() & AUTH_PIN) != 0;

  ////////
  // Animate the user info (ie, icon, name) up or down the screen.

  int non_pin_y_end_in_screen = GetBoundsInScreen().y();

  // Transform the layer so the user view renders where it used to be. This
  // requires a y offset.
  // Note: Doing this animation via ui::ScopedLayerAnimationSettings works, but
  // it seems that the timing gets slightly out of sync with the PIN animation.
  auto move_to_center = std::make_unique<ui::InterpolatedTranslation>(
      gfx::PointF(0, cached_animation_state_->non_pin_y_start_in_screen -
                         non_pin_y_end_in_screen),
      gfx::PointF());
  auto transition =
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::move(move_to_center),
          base::TimeDelta::FromMilliseconds(
              login_constants::kChangeUserAnimationDurationMs));
  transition->set_tween_type(gfx::Tween::Type::FAST_OUT_SLOW_IN);
  auto* sequence = new ui::LayerAnimationSequence(std::move(transition));
  layer()->GetAnimator()->StartAnimation(sequence);

  ////////
  // Fade the password view if it is being hidden or shown.

  if (cached_animation_state_->had_password != has_password) {
    float opacity_start = 0, opacity_end = 1;
    if (!has_password)
      std::swap(opacity_start, opacity_end);

    password_view_->layer()->SetOpacity(opacity_start);

    {
      ui::ScopedLayerAnimationSettings settings(
          password_view_->layer()->GetAnimator());
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
          login_constants::kChangeUserAnimationDurationMs));
      settings.SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);

      password_view_->layer()->SetOpacity(opacity_end);
    }
  }

  ////////
  // Grow/shrink the PIN keyboard if it is being hidden or shown.

  if (cached_animation_state_->had_pin != has_pin) {
    if (!has_pin) {
      gfx::Point pin_end_in_screen = pin_view_->GetBoundsInScreen().origin();
      gfx::Rect pin_bounds = pin_view_->bounds();
      pin_bounds.set_x(cached_animation_state_->pin_start_in_screen.x() -
                       pin_end_in_screen.x());
      pin_bounds.set_y(cached_animation_state_->pin_start_in_screen.y() -
                       pin_end_in_screen.y());

      // Since PIN is disabled, the previous Layout() hid the PIN keyboard.
      // We need to redisplay it where it used to be.
      pin_view_->SetVisible(true);
      pin_view_->SetBoundsRect(pin_bounds);
    }

    auto transition = std::make_unique<PinKeyboardAnimation>(
        has_pin /*grow*/, pin_view_->height(),
        base::TimeDelta::FromMilliseconds(
            login_constants::kChangeUserAnimationDurationMs),
        gfx::Tween::FAST_OUT_SLOW_IN);
    auto* sequence = new ui::LayerAnimationSequence(std::move(transition));
    pin_view_->layer()->GetAnimator()->ScheduleAnimation(sequence);

    // Hide the PIN keyboard after animation if needed.
    if (!has_pin) {
      auto* observer = BuildObserverToHideView(pin_view_);
      sequence->AddObserver(observer);
      observer->SetActive();
    }
  }

  cached_animation_state_.reset();
}

void LoginAuthUserView::UpdateForUser(const mojom::LoginUserInfoPtr& user) {
  user_view_->UpdateForUser(user, true /*animate*/);
  password_view_->UpdateForUser(user);
  password_view_->Clear();
}

const mojom::LoginUserInfoPtr& LoginAuthUserView::current_user() const {
  return user_view_->current_user();
}

gfx::Size LoginAuthUserView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  // Make sure we are at least as big as the user view. If we do not do this the
  // view will be below minimum size when no auth methods are displayed.
  size.SetToMax(user_view_->GetPreferredSize());
  return size;
}

void LoginAuthUserView::RequestFocus() {
  password_view_->RequestFocus();
}

void LoginAuthUserView::OnAuthSubmit(const base::string16& password) {
  bool authenticated_by_pin = (auth_methods_ & AUTH_PIN) != 0;

  password_view_->SetReadOnly(true);
  Shell::Get()->login_screen_controller()->AuthenticateUser(
      current_user()->basic_user_info->account_id, base::UTF16ToUTF8(password),
      authenticated_by_pin,
      base::BindOnce(&LoginAuthUserView::OnAuthComplete,
                     weak_factory_.GetWeakPtr()));
}

void LoginAuthUserView::OnAuthComplete(base::Optional<bool> auth_success) {
  if (!auth_success.has_value())
    return;

  // Clear the password only if auth fails. Make sure to keep the password view
  // disabled even if auth succeededs, as if the user submits a password while
  // animating the next lock screen will not work as expected. See
  // https://crbug.com/808486.
  if (!auth_success.value()) {
    password_view_->Clear();
    password_view_->SetReadOnly(false);
  }

  on_auth_.Run(auth_success.value());
}

void LoginAuthUserView::OnUserViewTap() {
  if (HasAuthMethod(AUTH_TAP)) {
    Shell::Get()->login_screen_controller()->AttemptUnlock(
        current_user()->basic_user_info->account_id);
  } else {
    on_tap_.Run();
  }
}

bool LoginAuthUserView::HasAuthMethod(AuthMethods auth_method) const {
  return (auth_methods_ & auth_method) != 0;
}

}  // namespace ash
