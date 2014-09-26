// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/new_avatar_button.h"

#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/painter.h"

namespace {

scoped_ptr<views::Border> CreateBorder(const int normal_image_set[],
                                       const int hot_image_set[],
                                       const int pushed_image_set[]) {
  scoped_ptr<views::LabelButtonBorder> border(
      new views::LabelButtonBorder(views::Button::STYLE_TEXTBUTTON));
  border->SetPainter(false, views::Button::STATE_NORMAL,
      views::Painter::CreateImageGridPainter(normal_image_set));
  border->SetPainter(false, views::Button::STATE_HOVERED,
      views::Painter::CreateImageGridPainter(hot_image_set));
  border->SetPainter(false, views::Button::STATE_PRESSED,
      views::Painter::CreateImageGridPainter(pushed_image_set));

  const int kLeftRightInset = 8;
  const int kTopInset = 2;
  const int kBottomInset = 4;
  border->set_insets(gfx::Insets(kTopInset, kLeftRightInset,
                                 kBottomInset, kLeftRightInset));

  return border.PassAs<views::Border>();
}

}  // namespace

NewAvatarButton::NewAvatarButton(views::ButtonListener* listener,
                                 AvatarButtonStyle button_style,
                                 Browser* browser)
    : LabelButton(listener, base::string16()),
      browser_(browser),
      has_auth_error_(false),
      suppress_mouse_released_action_(false) {
  set_animate_on_state_change(false);
  SetTextColor(views::Button::STATE_NORMAL, SK_ColorWHITE);
  SetTextColor(views::Button::STATE_HOVERED, SK_ColorWHITE);
  SetTextColor(views::Button::STATE_PRESSED, SK_ColorWHITE);
  SetTextShadows(gfx::ShadowValues(10,
      gfx::ShadowValue(gfx::Point(), 1.0f, SK_ColorDKGRAY)));
  SetTextSubpixelRenderingEnabled(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  // The largest text height that fits in the button. If the font list height
  // is larger than this, it will be shrunk to match it.
  // TODO(noms): Calculate this constant algorithmically.
  const int kDisplayFontHeight = 15;
  SetFontList(GetFontList().DeriveWithHeightUpperBound(kDisplayFontHeight));

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  if (button_style == THEMED_BUTTON) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    generic_avatar_ =
        *rb->GetImageNamed(IDR_AVATAR_THEMED_BUTTON_AVATAR).ToImageSkia();
#if defined(OS_WIN)
  } else if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    generic_avatar_ =
        *rb->GetImageNamed(IDR_AVATAR_METRO_BUTTON_AVATAR).ToImageSkia();
#endif
  } else {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    generic_avatar_ =
        *rb->GetImageNamed(IDR_AVATAR_GLASS_BUTTON_AVATAR).ToImageSkia();
  }

  g_browser_process->profile_manager()->GetProfileInfoCache().AddObserver(this);

  // Subscribe to authentication error changes so that the avatar button can
  // update itself.  Note that guest mode profiles won't have a token service.
  SigninErrorController* error =
      profiles::GetSigninErrorController(browser_->profile());
  if (error) {
    error->AddObserver(this);
    // This calls UpdateAvatarButtonAndRelayoutParent().
    OnErrorChanged();
  } else {
    UpdateAvatarButtonAndRelayoutParent();
  }
  SchedulePaint();
}

NewAvatarButton::~NewAvatarButton() {
  g_browser_process->profile_manager()->
      GetProfileInfoCache().RemoveObserver(this);
  SigninErrorController* error =
      profiles::GetSigninErrorController(browser_->profile());
  if (error)
    error->RemoveObserver(this);
}

bool NewAvatarButton::OnMousePressed(const ui::MouseEvent& event) {
  // Prevent the bubble from being re-shown if it's already showing.
  suppress_mouse_released_action_ = ProfileChooserView::IsShowing();
  return LabelButton::OnMousePressed(event);
}

void NewAvatarButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (suppress_mouse_released_action_)
    suppress_mouse_released_action_ = false;
  else
    LabelButton::OnMouseReleased(event);
}

void NewAvatarButton::OnProfileAdded(const base::FilePath& profile_path) {
  UpdateAvatarButtonAndRelayoutParent();
}

void NewAvatarButton::OnProfileWasRemoved(
      const base::FilePath& profile_path,
      const base::string16& profile_name) {
  UpdateAvatarButtonAndRelayoutParent();
}

void NewAvatarButton::OnProfileNameChanged(
      const base::FilePath& profile_path,
      const base::string16& old_profile_name) {
  UpdateAvatarButtonAndRelayoutParent();
}

void NewAvatarButton::OnProfileAvatarChanged(
      const base::FilePath& profile_path) {
  UpdateAvatarButtonAndRelayoutParent();
}

void NewAvatarButton::OnProfileSupervisedUserIdChanged(
      const base::FilePath& profile_path) {
  UpdateAvatarButtonAndRelayoutParent();
}

void NewAvatarButton::OnErrorChanged() {
  // If there is an error, show an warning icon.
  const SigninErrorController* error =
      profiles::GetSigninErrorController(browser_->profile());
  has_auth_error_ = error && error->HasError();

  UpdateAvatarButtonAndRelayoutParent();
}

void NewAvatarButton::UpdateAvatarButtonAndRelayoutParent() {
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();

  // If we have a single local profile, then use the generic avatar
  // button instead of the profile name. Never use the generic button if
  // the active profile is Guest.
  bool use_generic_button = (!browser_->profile()->IsGuestSession() &&
                             cache.GetNumberOfProfiles() == 1 &&
                             cache.GetUserNameOfProfileAtIndex(0).empty());

  SetText(use_generic_button ? base::string16() :
      profiles::GetAvatarButtonTextForProfile(browser_->profile()));
  // We want the button to resize if the new text is shorter.
  SetMinSize(gfx::Size());

  if (use_generic_button) {
    SetImage(views::Button::STATE_NORMAL, generic_avatar_);
  } else if (has_auth_error_) {
    SetImage(views::Button::STATE_NORMAL,
             *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                  IDR_ICON_PROFILES_AVATAR_BUTTON_ERROR).ToImageSkia());
  } else {
    SetImage(views::Button::STATE_NORMAL, gfx::ImageSkia());
  }

  // If we are not using the generic button, then reset the spacing between
  // the text and the possible authentication error icon.
  const int kDefaultImageTextSpacing = 5;
  SetImageLabelSpacing(use_generic_button ? 0 : kDefaultImageTextSpacing);

  InvalidateLayout();

  // Because the width of the button might have changed, the parent browser
  // frame needs to recalculate the button bounds and redraw it.
  if (parent())
    parent()->Layout();
}
