// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_user_view.h"

#include <memory>

#include "ash/ash_constants.h"
#include "ash/login/ui/animated_rounded_image_view.h"
#include "ash/login/ui/image_parser.h"
#include "ash/login/ui/login_bubble.h"
#include "ash/login/ui/login_constants.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/user_switch_flip_animation.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/user/rounded_image_view.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"

namespace ash {
namespace {

// Vertical spacing between icon, label, and authentication UI.
constexpr int kVerticalSpacingBetweenEntriesDp = 32;
// Horizontal spacing between username label and the dropdown icon.
constexpr int kDistanceBetweenUsernameAndDropdownDp = 12;
// Distance from the top of the user view to the user icon.
constexpr int kDistanceFromTopOfBigUserViewToUserIconDp = 54;
// Distance between user icon and the user label in small/extra-small layouts.
constexpr int kSmallManyDistanceFromUserIconToUserLabelDp = 16;

constexpr int kDropdownIconSizeDp = 20;

// Width/height of the user view. Ensures proper centering.
constexpr int kLargeUserViewWidthDp = 306;
constexpr int kLargeUserViewHeightDp = 346;
constexpr int kSmallUserViewWidthDp = 304;
constexpr int kExtraSmallUserViewWidthDp = 282;

// Width/height of the user image.
constexpr int kLargeUserImageSizeDp = 96;
constexpr int kSmallUserImageSizeDp = 74;
constexpr int kExtraSmallUserImageSizeDp = 60;

// Opacity for when the user view is active/focused and inactive.
constexpr float kOpaqueUserViewOpacity = 1.f;
constexpr float kTransparentUserViewOpacity = 0.63f;
constexpr float kUserFadeAnimationDurationMs = 180;

constexpr const char kUserViewClassName[] = "UserView";
constexpr const char kLoginUserImageClassName[] = "LoginUserImage";
constexpr const char kLoginUserLabelClassName[] = "LoginUserLabel";

int GetImageSize(LoginDisplayStyle style) {
  switch (style) {
    case LoginDisplayStyle::kLarge:
      return kLargeUserImageSizeDp;
    case LoginDisplayStyle::kSmall:
      return kSmallUserImageSizeDp;
    case LoginDisplayStyle::kExtraSmall:
      return kExtraSmallUserImageSizeDp;
      break;
  }

  NOTREACHED();
  return kLargeUserImageSizeDp;
}

views::View* MakePreferredSizeView(gfx::Size size) {
  auto* view = new NonAccessibleView();
  view->SetPreferredSize(size);
  return view;
}

}  // namespace

// Renders a user's profile icon.
class LoginUserView::UserImage : public NonAccessibleView {
 public:
  UserImage(int size)
      : NonAccessibleView(kLoginUserImageClassName),
        size_(size),
        weak_factory_(this) {
    SetLayoutManager(new views::FillLayout());

    // TODO(jdufault): We need to render a black border. We will probably have
    // to add support directly to AnimatedRoundedImageView, since the existing
    // views::Border renders based on bounds (ie, a rectangle).
    image_ = new AnimatedRoundedImageView(size_ / 2);
    AddChildView(image_);
  }
  ~UserImage() override = default;

  void UpdateForUser(const mojom::LoginUserInfoPtr& user) {
    // Set the initial image from |avatar| since we already have it available.
    // Then, decode the bytes via blink's PNG decoder and play any animated
    // frames if they are available.
    if (!user->basic_user_info->avatar.isNull())
      image_->SetImage(user->basic_user_info->avatar, gfx::Size(size_, size_));

    // Decode the avatar using blink, as blink's PNG decoder supports APNG,
    // which is the format used for the animated avators.
    if (!user->basic_user_info->avatar_bytes.empty()) {
      DecodeAnimation(user->basic_user_info->avatar_bytes,
                      base::Bind(&LoginUserView::UserImage::OnImageDecoded,
                                 weak_factory_.GetWeakPtr()));
    }
  }

  void SetAnimationEnabled(bool enable) { image_->SetAnimationEnabled(enable); }

 private:
  void OnImageDecoded(AnimationFrames animation) {
    // If there is only a single frame to display, show the existing avatar.
    if (animation.size() <= 1) {
      LOG_IF(ERROR, animation.empty()) << "Decoding user avatar failed";
      return;
    }

    image_->SetAnimation(animation, gfx::Size(size_, size_));
  }

  AnimatedRoundedImageView* image_ = nullptr;
  int size_;

  base::WeakPtrFactory<UserImage> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserImage);
};

// Shows the user's name.
class LoginUserView::UserLabel : public NonAccessibleView {
 public:
  UserLabel(LoginDisplayStyle style)
      : NonAccessibleView(kLoginUserLabelClassName) {
    SetLayoutManager(new views::FillLayout());

    user_name_ = new views::Label();
    user_name_->SetEnabledColor(SK_ColorWHITE);
    user_name_->SetSubpixelRenderingEnabled(false);
    user_name_->SetAutoColorReadabilityEnabled(false);

    // TODO(jdufault): Figure out the correct font.
    const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();

    switch (style) {
      case LoginDisplayStyle::kLarge:
        user_name_->SetFontList(base_font_list.Derive(
            11, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::LIGHT));
        break;
      case LoginDisplayStyle::kSmall:
        user_name_->SetFontList(base_font_list.Derive(
            8, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::LIGHT));
        break;
      case LoginDisplayStyle::kExtraSmall:
        // TODO(jdufault): match font against spec.
        user_name_->SetFontList(base_font_list.Derive(
            6, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::LIGHT));
        break;
    }

    AddChildView(user_name_);
  }
  ~UserLabel() override = default;

  void UpdateForUser(const mojom::LoginUserInfoPtr& user) {
    std::string display_name = user->basic_user_info->display_name;
    // display_name can be empty in debug builds with stub users.
    if (display_name.empty())
      display_name = user->basic_user_info->display_email;
    user_name_->SetText(base::UTF8ToUTF16(display_name));
  }

  const base::string16& displayed_name() const { return user_name_->text(); }

 private:
  views::Label* user_name_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UserLabel);
};

// LoginUserView is defined after LoginUserView::UserLabel so it can access the
// class members.

LoginUserView::TestApi::TestApi(LoginUserView* view) : view_(view) {}

LoginUserView::TestApi::~TestApi() = default;

LoginDisplayStyle LoginUserView::TestApi::display_style() const {
  return view_->display_style_;
}

const base::string16& LoginUserView::TestApi::displayed_name() const {
  return view_->user_label_->displayed_name();
}

views::View* LoginUserView::TestApi::user_label() const {
  return view_->user_label_;
}

bool LoginUserView::TestApi::is_opaque() const {
  return view_->is_opaque_;
}

// Opacity updates are dispatched via a PreTarget event handler to ensure that
// LoginUserView receives an opacity update for every event, even if the event
// is handled elsewhere.
class LoginUserView::OpacityInputHandler : public ui::EventHandler {
 public:
  explicit OpacityInputHandler(LoginUserView* user_view)
      : user_view_(user_view) {}

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    user_view_->UpdateOpacity();
  }

 private:
  LoginUserView* user_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(OpacityInputHandler);
};

// static
int LoginUserView::WidthForLayoutStyle(LoginDisplayStyle style) {
  switch (style) {
    case LoginDisplayStyle::kLarge:
      return kLargeUserViewWidthDp;
    case LoginDisplayStyle::kSmall:
      return kSmallUserViewWidthDp;
    case LoginDisplayStyle::kExtraSmall:
      return kExtraSmallUserViewWidthDp;
  }

  NOTREACHED();
  return 0;
}

LoginUserView::LoginUserView(LoginDisplayStyle style,
                             bool show_dropdown,
                             const OnTap& on_tap)
    : views::Button(this),
      on_tap_(on_tap),
      opacity_input_handler_(std::make_unique<OpacityInputHandler>(this)),
      display_style_(style) {
  // show_dropdown can only be true when the user view is rendering in large
  // mode.
  DCHECK(!show_dropdown || style == LoginDisplayStyle::kLarge);

  user_image_ = new UserImage(GetImageSize(style));
  user_label_ = new UserLabel(style);
  if (show_dropdown) {
    user_dropdown_ = new views::ImageButton(this);
    user_dropdown_->SetPreferredSize(
        gfx::Size(kDropdownIconSizeDp, kDropdownIconSizeDp));
    user_dropdown_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kLockScreenDropdownIcon, SK_ColorWHITE));
    user_dropdown_->SetFocusBehavior(FocusBehavior::ALWAYS);
    user_dropdown_->SetFocusPainter(views::Painter::CreateSolidFocusPainter(
        kFocusBorderColor, kFocusBorderThickness, gfx::InsetsF()));
  }

  switch (style) {
    case LoginDisplayStyle::kLarge:
      SetLargeLayout();
      break;
    case LoginDisplayStyle::kSmall:
    case LoginDisplayStyle::kExtraSmall:
      SetSmallishLayout();
      break;
  }

  // Layer rendering is needed for animation. We apply animations to child views
  // separately to reduce overdraw.
  auto setup_layer = [](views::View* view) {
    view->SetPaintToLayer();
    view->layer()->SetFillsBoundsOpaquely(false);
    view->layer()->SetOpacity(kTransparentUserViewOpacity);
    view->layer()->GetAnimator()->set_preemption_strategy(
        ui::LayerAnimator::PreemptionStrategy::
            IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  };
  setup_layer(user_image_);
  setup_layer(user_label_);
  if (user_dropdown_)
    setup_layer(user_dropdown_);

  AddPreTargetHandler(opacity_input_handler_.get());
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

LoginUserView::~LoginUserView() {
  RemovePreTargetHandler(opacity_input_handler_.get());
}

void LoginUserView::UpdateForUser(const mojom::LoginUserInfoPtr& user,
                                  bool animate) {
  current_user_ = user->Clone();

  if (animate) {
    // Stop any existing animation.
    user_image_->layer()->GetAnimator()->StopAnimating();

    // Create the image flip animation.
    auto image_transition = std::make_unique<UserSwitchFlipAnimation>(
        user_image_->width(), 0 /*start_degrees*/, 90 /*midpoint_degrees*/,
        180 /*end_degrees*/,
        base::TimeDelta::FromMilliseconds(
            login_constants::kChangeUserAnimationDurationMs),
        gfx::Tween::Type::EASE_OUT,
        base::BindOnce(&LoginUserView::UpdateCurrentUserState,
                       base::Unretained(this)));
    auto* image_sequence =
        new ui::LayerAnimationSequence(std::move(image_transition));
    user_image_->layer()->GetAnimator()->StartAnimation(image_sequence);

    // Create opacity fade animation, which applies to the entire element.
    bool is_opaque = this->is_opaque_;
    auto make_opacity_sequence = [is_opaque]() {
      auto make_opacity_element = [](float target_opacity) {
        auto element = ui::LayerAnimationElement::CreateOpacityElement(
            target_opacity,
            base::TimeDelta::FromMilliseconds(
                login_constants::kChangeUserAnimationDurationMs / 2.0f));
        element->set_tween_type(gfx::Tween::Type::EASE_OUT);
        return element;
      };

      auto* opacity_sequence = new ui::LayerAnimationSequence();
      opacity_sequence->AddElement(make_opacity_element(0 /*target_opacity*/));
      opacity_sequence->AddElement(make_opacity_element(
          is_opaque ? kOpaqueUserViewOpacity
                    : kTransparentUserViewOpacity /*target_opacity*/));
      return opacity_sequence;
    };
    user_image_->layer()->GetAnimator()->StartAnimation(
        make_opacity_sequence());
    user_label_->layer()->GetAnimator()->StartAnimation(
        make_opacity_sequence());
    if (user_dropdown_) {
      user_dropdown_->layer()->GetAnimator()->StartAnimation(
          make_opacity_sequence());
    }
  } else {
    // Do not animate, so directly update to the current user.
    UpdateCurrentUserState();
  }
}

void LoginUserView::SetForceOpaque(bool force_opaque) {
  force_opaque_ = force_opaque;
  UpdateOpacity();
}

const char* LoginUserView::GetClassName() const {
  return kUserViewClassName;
}

gfx::Size LoginUserView::CalculatePreferredSize() const {
  switch (display_style_) {
    case LoginDisplayStyle::kLarge:
      return gfx::Size(kLargeUserViewWidthDp, kLargeUserViewHeightDp);
    case LoginDisplayStyle::kSmall:
      return gfx::Size(kSmallUserViewWidthDp, kSmallUserImageSizeDp);
    case LoginDisplayStyle::kExtraSmall:
      return gfx::Size(kExtraSmallUserViewWidthDp, kExtraSmallUserImageSizeDp);
  }

  NOTREACHED();
  return gfx::Size();
}

void LoginUserView::OnFocus() {
  views::Button::OnFocus();
  UpdateOpacity();
}

void LoginUserView::OnBlur() {
  views::Button::OnBlur();
  UpdateOpacity();
}

void LoginUserView::ButtonPressed(Button* sender, const ui::Event& event) {
  on_tap_.Run();
  if (user_dropdown_ && sender == user_dropdown_) {
    if (!user_menu_ || !user_menu_->IsVisible()) {
      user_menu_ = std::make_unique<LoginBubble>();
      base::string16 display_name =
          base::UTF8ToUTF16(current_user_->basic_user_info->display_name);

      user_menu_->ShowUserMenu(
          current_user_->is_device_owner
              ? l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_POD_OWNER_USER,
                                           display_name)
              : display_name,
          base::UTF8ToUTF16(current_user_->basic_user_info->display_email),
          user_dropdown_ /*anchor_view*/, user_dropdown_ /*bubble_opener*/,
          false /*show_remove_user*/);
    } else {
      user_menu_->Close();
    }
  }
}

void LoginUserView::UpdateCurrentUserState() {
  SetAccessibleName(
      base::UTF8ToUTF16(current_user_->basic_user_info->display_email));
  user_image_->UpdateForUser(current_user_);
  user_label_->UpdateForUser(current_user_);
  Layout();
}

void LoginUserView::UpdateOpacity() {
  bool was_opaque = is_opaque_;
  is_opaque_ = force_opaque_ || IsMouseHovered() || HasFocus();

  if (was_opaque == is_opaque_)
    return;

  // Animate to new opacity.
  auto build_settings = [](views::View* view) {
    auto settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        view->layer()->GetAnimator());
    settings->SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kUserFadeAnimationDurationMs));
    settings->SetTweenType(gfx::Tween::Type::EASE_IN_OUT);
    return settings;
  };
  auto user_image_settings = build_settings(user_image_);
  auto user_label_settings = build_settings(user_label_);
  float target_opacity =
      is_opaque_ ? kOpaqueUserViewOpacity : kTransparentUserViewOpacity;
  user_image_->layer()->SetOpacity(target_opacity);
  user_label_->layer()->SetOpacity(target_opacity);
  if (user_dropdown_) {
    auto user_dropdown_settings = build_settings(user_dropdown_);
    user_dropdown_->layer()->SetOpacity(target_opacity);
  }

  // Animate avatar only if we are opaque.
  user_image_->SetAnimationEnabled(is_opaque_);
}

void LoginUserView::SetLargeLayout() {
  auto* root_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(),
                           kVerticalSpacingBetweenEntriesDp);
  SetLayoutManager(root_layout);

  // Space between top of user view and user icon.
  AddChildView(MakePreferredSizeView(
      gfx::Size(0, kDistanceFromTopOfBigUserViewToUserIconDp -
                       kVerticalSpacingBetweenEntriesDp)));

  // Centered user image
  {
    auto* row = new NonAccessibleView();
    AddChildView(row);

    auto* layout = new views::BoxLayout(views::BoxLayout::kHorizontal);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    row->SetLayoutManager(layout);

    row->AddChildView(user_image_);
  }

  // User name, menu dropdown
  {
    auto* row = new NonAccessibleView();
    AddChildView(row);

    auto* layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, gfx::Insets(),
                             kDistanceBetweenUsernameAndDropdownDp);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    row->SetLayoutManager(layout);

    // Add an empty view that has the same size as the dropdown so we center on
    // |user_label_|. This is simpler than doing manual size calculation to take
    // into account the extra offset.
    if (user_dropdown_) {
      row->AddChildView(
          MakePreferredSizeView(user_dropdown_->GetPreferredSize()));
    }
    row->AddChildView(user_label_);
    if (user_dropdown_)
      row->AddChildView(user_dropdown_);
  }
}

void LoginUserView::SetSmallishLayout() {
  auto* root_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, gfx::Insets(),
                           kSmallManyDistanceFromUserIconToUserLabelDp);
  SetLayoutManager(root_layout);

  AddChildView(user_image_);
  AddChildView(user_label_);
}

}  // namespace ash
