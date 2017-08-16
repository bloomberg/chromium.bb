// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_user_view.h"

#include "ash/login/ui/login_constants.h"
#include "ash/login/ui/user_switch_flip_animation.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/user/rounded_image_view.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

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

constexpr const char* kUserViewClassName = "UserView";
constexpr const char* kLoginUserImageClassName = "LoginUserImage";
constexpr const char* kLoginUserLabelClassName = "LoginUserLabel";

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
  auto* view = new views::View();
  view->SetPreferredSize(size);
  return view;
}

}  // namespace

// Renders a user's profile icon.
class LoginUserView::UserImage : public views::View {
 public:
  UserImage(int size) : size_(size) {
    SetLayoutManager(new views::FillLayout());

    // TODO(jdufault): We need to render a black border. We will probably have
    // to add support directly to RoundedImageView, since the existing
    // views::Border renders based on bounds (ie, a rectangle).
    image_ = new ash::tray::RoundedImageView(size_ / 2);
    AddChildView(image_);
  }
  ~UserImage() override = default;

  void UpdateForUser(const mojom::UserInfoPtr& user) {
    image_->SetImage(user->avatar, gfx::Size(size_, size_));
  }

  // views::View:
  const char* GetClassName() const override { return kLoginUserImageClassName; }

 private:
  ash::tray::RoundedImageView* image_ = nullptr;
  int size_;

  DISALLOW_COPY_AND_ASSIGN(UserImage);
};

// Shows the user's name.
class LoginUserView::UserLabel : public views::View {
 public:
  UserLabel(LoginDisplayStyle style) {
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

  void UpdateForUser(const mojom::UserInfoPtr& user) {
    std::string display_name = user->display_name;
    // display_name can be empty in debug builds with stub users.
    if (display_name.empty())
      display_name = user->display_email;
    user_name_->SetText(base::UTF8ToUTF16(display_name));
  }

  const base::string16& displayed_name() const { return user_name_->text(); }

  // views::View:
  const char* GetClassName() const override { return kLoginUserLabelClassName; }

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
    : views::CustomButton(this), on_tap_(on_tap), display_style_(style) {
  // show_dropdown can only be true when the user view is rendering in large
  // mode.
  DCHECK(!show_dropdown || style == LoginDisplayStyle::kLarge);

  user_image_ = new UserImage(GetImageSize(style));
  user_label_ = new UserLabel(style);
  if (show_dropdown) {
    user_dropdown_ = new views::ImageView();
    user_dropdown_->SetPreferredSize(
        gfx::Size(kDropdownIconSizeDp, kDropdownIconSizeDp));
    user_dropdown_->SetImage(
        gfx::CreateVectorIcon(kLockScreenDropdownIcon, SK_ColorWHITE));
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
  user_image_->SetPaintToLayer();
  user_image_->layer()->SetFillsBoundsOpaquely(false);

  user_label_->SetPaintToLayer();
  user_label_->layer()->SetFillsBoundsOpaquely(false);

  if (user_dropdown_) {
    user_dropdown_->SetPaintToLayer();
    user_dropdown_->layer()->SetFillsBoundsOpaquely(false);
  }
}

LoginUserView::~LoginUserView() = default;

void LoginUserView::UpdateForUser(const mojom::UserInfoPtr& user,
                                  bool animate) {
  current_user_ = user->Clone();

  if (animate) {
    // Stop any existing animation.
    user_image_->layer()->GetAnimator()->StopAnimating();

    // Create the image flip animation.
    auto image_transition = base::MakeUnique<UserSwitchFlipAnimation>(
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
    auto make_opacity_sequence = []() {
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
      opacity_sequence->AddElement(make_opacity_element(1 /*target_opacity*/));
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

void LoginUserView::ButtonPressed(Button* sender, const ui::Event& event) {
  on_tap_.Run();
}

void LoginUserView::UpdateCurrentUserState() {
  user_image_->UpdateForUser(current_user_);
  user_label_->UpdateForUser(current_user_);
  Layout();
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
    auto* row = new views::View();
    AddChildView(row);

    auto* layout = new views::BoxLayout(views::BoxLayout::kHorizontal);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    row->SetLayoutManager(layout);

    row->AddChildView(user_image_);
  }

  // User name, menu dropdown
  {
    auto* row = new views::View();
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

}  // namespace ash