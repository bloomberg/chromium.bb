// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/new_avatar_button.h"

#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
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

  const int kLeftRightInset = 10;
  const int kTopInset = 0;
  const int kBottomInset = 4;
  border->set_insets(gfx::Insets(kTopInset, kLeftRightInset,
                                 kBottomInset, kLeftRightInset));

  return border.PassAs<views::Border>();
}

base::string16 GetElidedText(const base::string16& original_text) {
  // Maximum characters the button can be before the text will get elided.
  const int kMaxCharactersToDisplay = 15;
  const gfx::FontList font_list;
  return gfx::ElideText(original_text, font_list,
                        font_list.GetExpectedTextWidth(kMaxCharactersToDisplay),
                        gfx::ELIDE_TAIL);
}

base::string16 GetButtonText(Profile* profile) {
  base::string16 name = GetElidedText(profiles::GetAvatarNameForProfile(
      profile->GetPath()));
  if (profile->IsSupervised())
    name = l10n_util::GetStringFUTF16(IDS_SUPERVISED_USER_NEW_AVATAR_LABEL,
                                      name);
  return name;
}

}  // namespace

NewAvatarButton::NewAvatarButton(
    views::ButtonListener* listener,
    const base::string16& profile_name,
    AvatarButtonStyle button_style,
    Browser* browser)
    : MenuButton(listener, GetButtonText(browser->profile()), NULL, true),
      browser_(browser) {
  set_animate_on_state_change(false);
  SetTextColor(views::Button::STATE_NORMAL, SK_ColorWHITE);
  SetTextColor(views::Button::STATE_HOVERED, SK_ColorWHITE);
  SetTextColor(views::Button::STATE_PRESSED, SK_ColorWHITE);
  SetTextShadows(gfx::ShadowValues(10,
      gfx::ShadowValue(gfx::Point(), 1.0f, SK_ColorDKGRAY)));
  SetTextSubpixelRenderingEnabled(false);

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  if (button_style == THEMED_BUTTON) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    set_menu_marker(
        rb->GetImageNamed(IDR_AVATAR_THEMED_BUTTON_DROPARROW).ToImageSkia());
#if defined(OS_WIN)
  } else if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    set_menu_marker(
        rb->GetImageNamed(IDR_AVATAR_METRO_BUTTON_DROPARROW).ToImageSkia());
#endif
  } else {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    set_menu_marker(
        rb->GetImageNamed(IDR_AVATAR_GLASS_BUTTON_DROPARROW).ToImageSkia());
  }

  g_browser_process->profile_manager()->GetProfileInfoCache().AddObserver(this);

  // Subscribe to authentication error changes so that the avatar button can
  // update itself.  Note that guest mode profiles won't have a token service.
  SigninErrorController* error =
      profiles::GetSigninErrorController(browser_->profile());
  if (error) {
    error->AddObserver(this);
    OnErrorChanged();
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

void NewAvatarButton::OnProfileSupervisedUserIdChanged(
      const base::FilePath& profile_path) {
  UpdateAvatarButtonAndRelayoutParent();
}

void NewAvatarButton::OnErrorChanged() {
  gfx::ImageSkia icon;

  // If there is an error, show an warning icon.
  const SigninErrorController* error =
      profiles::GetSigninErrorController(browser_->profile());
  if (error && error->HasError()) {
    icon = *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_ICON_PROFILES_AVATAR_BUTTON_ERROR).ToImageSkia();
  }

  SetImage(views::Button::STATE_NORMAL, icon);
  UpdateAvatarButtonAndRelayoutParent();
}

void NewAvatarButton::UpdateAvatarButtonAndRelayoutParent() {
  // We want the button to resize if the new text is shorter.
  SetText(GetButtonText(browser_->profile()));
  set_min_size(gfx::Size());
  InvalidateLayout();

  // Because the width of the button might have changed, the parent browser
  // frame needs to recalculate the button bounds and redraw it.
  if (parent())
    parent()->Layout();
}
