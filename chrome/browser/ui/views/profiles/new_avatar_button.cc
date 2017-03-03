// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/new_avatar_button.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/views/profiles/avatar_button_delegate.h"
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/vector_icons/vector_icons.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/painter.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

std::unique_ptr<views::Border> CreateBorder(const int normal_image_set[],
                                            const int hot_image_set[],
                                            const int pushed_image_set[]) {
  std::unique_ptr<views::LabelButtonAssetBorder> border(
      new views::LabelButtonAssetBorder(views::Button::STYLE_TEXTBUTTON));
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

  return std::move(border);
}

}  // namespace

NewAvatarButton::NewAvatarButton(AvatarButtonDelegate* delegate,
                                 AvatarButtonStyle button_style,
                                 Profile* profile)
    : LabelButton(delegate, base::string16()),
      delegate_(delegate),
      error_controller_(this, profile),
      profile_(profile),
      suppress_mouse_released_action_(false) {
  set_triggerable_event_flags(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON);
  set_animate_on_state_change(false);
  SetEnabledTextColors(SK_ColorWHITE);
  SetTextSubpixelRenderingEnabled(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  // The largest text height that fits in the button. If the font list height
  // is larger than this, it will be shrunk to match it.
  // TODO(noms): Calculate this constant algorithmically from the button's size.
  const int kDisplayFontHeight = 16;
  SetFontList(
      label()->font_list().DeriveWithHeightUpperBound(kDisplayFontHeight));

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  if (button_style == AvatarButtonStyle::THEMED) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    generic_avatar_ =
        *rb->GetImageNamed(IDR_AVATAR_THEMED_BUTTON_AVATAR).ToImageSkia();
#if defined(OS_WIN)
  } else if (base::win::GetVersion() < base::win::VERSION_WIN8) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    generic_avatar_ =
        *rb->GetImageNamed(IDR_AVATAR_GLASS_BUTTON_AVATAR).ToImageSkia();
#endif
  } else {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_NATIVE_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_NATIVE_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_NATIVE_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    generic_avatar_ =
        *rb->GetImageNamed(IDR_AVATAR_NATIVE_BUTTON_AVATAR).ToImageSkia();
  }

  g_browser_process->profile_manager()->
      GetProfileAttributesStorage().AddObserver(this);
  Update();
  SchedulePaint();
}

NewAvatarButton::~NewAvatarButton() {
  g_browser_process->profile_manager()->
      GetProfileAttributesStorage().RemoveObserver(this);
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

void NewAvatarButton::OnGestureEvent(ui::GestureEvent* event) {
  // TODO(wjmaclean): The check for ET_GESTURE_LONG_PRESS is done here since
  // no other UI button based on CustomButton appears to handle mouse
  // right-click. If other cases are identified, it may make sense to move this
  // check to CustomButton.
  if (event->type() == ui::ET_GESTURE_LONG_PRESS)
    NotifyClick(*event);
  else
    LabelButton::OnGestureEvent(event);
}

void NewAvatarButton::OnAvatarErrorChanged() {
  Update();
}

void NewAvatarButton::OnProfileAdded(const base::FilePath& profile_path) {
  Update();
}

void NewAvatarButton::OnProfileWasRemoved(
      const base::FilePath& profile_path,
      const base::string16& profile_name) {
  // If deleting the active profile, don't bother updating the avatar
  // button, as the browser window is being closed anyway.
  if (profile_->GetPath() != profile_path)
    Update();
}

void NewAvatarButton::OnProfileNameChanged(
      const base::FilePath& profile_path,
      const base::string16& old_profile_name) {
  if (profile_->GetPath() == profile_path)
    Update();
}

void NewAvatarButton::OnProfileSupervisedUserIdChanged(
      const base::FilePath& profile_path) {
  if (profile_->GetPath() == profile_path)
    Update();
}

void NewAvatarButton::Update() {
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();

  // If we have a single local profile, then use the generic avatar
  // button instead of the profile name. Never use the generic button if
  // the active profile is Guest.
  const bool use_generic_button =
      !profile_->IsGuestSession() &&
      storage.GetNumberOfProfiles() == 1 &&
      !storage.GetAllProfilesAttributes().front()->IsAuthenticated();

  SetText(use_generic_button
              ? base::string16()
              : profiles::GetAvatarButtonTextForProfile(profile_));

  // If the button has no text, clear the text shadows to make sure the
  // image is centered correctly.
  SetTextShadows(
      use_generic_button
          ? gfx::ShadowValues()
          : gfx::ShadowValues(
                10, gfx::ShadowValue(gfx::Vector2d(), 2.0f, SK_ColorDKGRAY)));

  // We want the button to resize if the new text is shorter.
  SetMinSize(gfx::Size());

  if (use_generic_button) {
    SetImage(views::Button::STATE_NORMAL, generic_avatar_);
  } else if (error_controller_.HasAvatarError()) {
    if (switches::IsMaterialDesignUserMenu()) {
      SetImage(views::Button::STATE_NORMAL,
               gfx::CreateVectorIcon(kSyncProblemIcon, 16, gfx::kGoogleRed700));
    } else {
      SetImage(
          views::Button::STATE_NORMAL,
          gfx::CreateVectorIcon(ui::kWarningIcon, 13, gfx::kGoogleYellow700));
    }
  } else {
    SetImage(views::Button::STATE_NORMAL, gfx::ImageSkia());
  }

  // If we are not using the generic button, then reset the spacing between
  // the text and the possible authentication error icon.
  const int kDefaultImageTextSpacing = 5;
  SetImageLabelSpacing(use_generic_button ? 0 : kDefaultImageTextSpacing);

  PreferredSizeChanged();
  delegate_->ButtonPreferredSizeChanged();
}
