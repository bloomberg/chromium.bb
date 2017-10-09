// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_user_view.h"

#include <memory>

#include "ash/login/lock_screen_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_constants.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/pin_keyboard_animation.h"
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
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr const char kNonPinRootViewName[] = "NonPinRootView";
constexpr const char kLoginAuthUserViewClassName[] = "LoginAuthUserView";

// Any non-zero value used for separator width. Makes debugging easier.
constexpr int kNonEmptyWidthDp = 30;

// Distance between the username label and the password textfield.
const int kDistanceBetweenUsernameAndPasswordDp = 28;

// Distance between the password textfield and the the pin keyboard.
const int kDistanceBetweenPasswordFieldAndPinKeyboard = 20;

// Distance from the end of pin keyboard to the bottom of the big user view.
const int kDistanceFromPinKeyboardToBigUserViewBottom = 48;

views::View* CreateViewOfHeight(int height) {
  auto* view = new NonAccessibleView();
  view->SetPreferredSize(gfx::Size(kNonEmptyWidthDp, height));
  return view;
}

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
  int pin_y_start_in_screen = 0;
  bool had_pin = false;
  bool had_password = false;

  explicit AnimationState(LoginAuthUserView* view) {
    non_pin_y_start_in_screen = view->non_pin_root_->GetBoundsInScreen().y();
    pin_y_start_in_screen = view->pin_view_->GetBoundsInScreen().y();

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

LoginAuthUserView::LoginAuthUserView(const mojom::LoginUserInfoPtr& user,
                                     const OnAuthCallback& on_auth,
                                     const LoginUserView::OnTap& on_tap)
    : NonAccessibleView(kLoginAuthUserViewClassName), on_auth_(on_auth) {
  // Build child views.
  user_view_ = new LoginUserView(LoginDisplayStyle::kLarge,
                                 true /*show_dropdown*/, on_tap);
  password_view_ = new LoginPasswordView();
  // Enable layer rendering so the password opacity can be animated.
  password_view_->SetPaintToLayer();
  password_view_->layer()->SetFillsBoundsOpaquely(false);
  password_view_->UpdateForUser(user);
  pin_view_ =
      new LoginPinView(base::BindRepeating(&LoginPasswordView::AppendNumber,
                                           base::Unretained(password_view_)),
                       base::BindRepeating(&LoginPasswordView::Backspace,
                                           base::Unretained(password_view_)));
  DCHECK(pin_view_->layer());
  // Initialization of |password_view_| is deferred because it needs the
  // |pin_view_| pointer.
  password_view_->Init(
      base::Bind(&LoginAuthUserView::OnAuthSubmit, base::Unretained(this)),
      base::Bind(&LoginPinView::OnPasswordTextChanged,
                 base::Unretained(pin_view_)));

  // Build layout.
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical));

  non_pin_root_ = new NonAccessibleView(kNonPinRootViewName);
  non_pin_root_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(),
                           kDistanceBetweenUsernameAndPasswordDp));
  // Non-PIN content will be painted outside of its bounds as it animates
  // to the correct position. This requires a layer to display properly.
  non_pin_root_->SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);
  AddChildView(non_pin_root_);

  SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);

  // Note: |user_view_| will be sized to it's minimum size (not its preferred
  // size) because of the vertical box layout manager. This class expresses the
  // minimum preferred size again so everything works out as desired (ie, we can
  // control how far away the password auth is from the user label).
  non_pin_root_->AddChildView(user_view_);

  AddChildView(CreateViewOfHeight(kDistanceBetweenUsernameAndPasswordDp));

  {
    // We need to center LoginPasswordAuth.
    //
    // Also, BoxLayout::kVertical will ignore preferred width, which messes up
    // separator rendering.
    auto* row = new NonAccessibleView();
    non_pin_root_->AddChildView(row);

    auto* layout = new views::BoxLayout(views::BoxLayout::kHorizontal);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    row->SetLayoutManager(layout);

    row->AddChildView(password_view_);
  }

  AddChildView(CreateViewOfHeight(kDistanceBetweenPasswordFieldAndPinKeyboard));

  {
    // We need to center LoginPinAuth.
    auto* row = new NonAccessibleView();
    AddChildView(row);

    auto* layout = new views::BoxLayout(views::BoxLayout::kHorizontal);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    row->SetLayoutManager(layout);

    row->AddChildView(pin_view_);
  }

  AddChildView(CreateViewOfHeight(kDistanceFromPinKeyboardToBigUserViewBottom));

  SetAuthMethods(auth_methods_);
  user_view_->UpdateForUser(user, false /*animate*/);
}

LoginAuthUserView::~LoginAuthUserView() = default;

void LoginAuthUserView::SetAuthMethods(uint32_t auth_methods) {
  // TODO(jdufault): Implement additional auth methods.
  auth_methods_ = static_cast<AuthMethods>(auth_methods);
  bool has_password = (auth_methods & AUTH_PASSWORD) != 0;
  bool has_pin = (auth_methods & AUTH_PIN) != 0;

  password_view_->SetEnabled(has_password);
  password_view_->SetFocusEnabledForChildViews(has_password);
  password_view_->layer()->SetOpacity(has_password ? 1 : 0);

  // Make sure to clear any existing password on showing the view. We do this on
  // show instead of on hide so that the password does not clear when animating
  // out.
  if (has_password) {
    password_view_->Clear();
    password_view_->RequestFocus();
  }

  pin_view_->SetVisible(has_pin);

  if (has_password && has_pin) {
    password_view_->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PIN_PLACEHOLDER));
  } else if (has_password) {
    password_view_->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PLACEHOLDER));
  }

  // Only the active auth user view has a password displayed. If that is the
  // case, then render the user view as if it was always focused, since clicking
  // on it will not do anything (such as swapping users).
  user_view_->SetForceOpaque(has_password);
  user_view_->SetFocusBehavior(has_password ? FocusBehavior::NEVER
                                            : FocusBehavior::ALWAYS);

  PreferredSizeChanged();
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
  non_pin_root_->layer()->GetAnimator()->AbortAllAnimations();

  bool has_password = (auth_methods() & AUTH_PASSWORD) != 0;
  bool has_pin = (auth_methods() & AUTH_PIN) != 0;

  ////////
  // Animate the user info (ie, icon, name) up or down the screen.

  int non_pin_y_end_in_screen = non_pin_root_->GetBoundsInScreen().y();

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
  non_pin_root_->layer()->GetAnimator()->StartAnimation(sequence);

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
      int pin_y_end_in_screen = pin_view_->GetBoundsInScreen().y();
      gfx::Rect pin_bounds = pin_view_->bounds();
      pin_bounds.set_y(cached_animation_state_->pin_y_start_in_screen -
                       pin_y_end_in_screen);

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
  Shell::Get()->lock_screen_controller()->AuthenticateUser(
      current_user()->basic_user_info->account_id, base::UTF16ToUTF8(password),
      (auth_methods_ & AUTH_PIN) != 0,
      base::BindOnce([](OnAuthCallback on_auth,
                        bool auth_success) { on_auth.Run(auth_success); },
                     on_auth_));
}

}  // namespace ash
