// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/new_avatar_button.h"

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
#include "ui/gfx/text_elider.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace {

// Text padding within the button border.
const int kInset = 10;

scoped_ptr<views::Border> CreateBorder(const int normal_image_set[],
                                       const int hot_image_set[],
                                       const int pushed_image_set[]) {
  scoped_ptr<views::TextButtonDefaultBorder> border(
      new views::TextButtonDefaultBorder());

  border->SetInsets(gfx::Insets(kInset, kInset, kInset, kInset));
  border->set_normal_painter(
      views::Painter::CreateImageGridPainter(normal_image_set));
  border->set_hot_painter(
      views::Painter::CreateImageGridPainter(hot_image_set));
  border->set_pushed_painter(
      views::Painter::CreateImageGridPainter(pushed_image_set));

  return border.PassAs<views::Border>();
}

base::string16 GetElidedText(const base::string16& original_text) {
  // Maximum characters the button can be before the text will get elided.
  const int kMaxCharactersToDisplay = 15;

  const gfx::FontList font_list;
  return gfx::ElideText(
      original_text,
      font_list,
      font_list.GetExpectedTextWidth(kMaxCharactersToDisplay),
      gfx::ELIDE_AT_END);
}

}  // namespace

NewAvatarButton::NewAvatarButton(
    views::ButtonListener* listener,
    const base::string16& profile_name,
    AvatarButtonStyle button_style,
    Browser* browser)
    : MenuButton(listener, GetElidedText(profile_name), NULL, true),
      browser_(browser) {
  set_animate_on_state_change(false);
  set_icon_placement(ICON_ON_RIGHT);

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();

  bool is_win8 = false;
#if defined(OS_WIN)
  is_win8 = base::win::GetVersion() >= base::win::VERSION_WIN8;
#endif

  if (button_style == THEMED_BUTTON) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    set_menu_marker(
        rb->GetImageNamed(IDR_AVATAR_THEMED_BUTTON_DROPARROW).ToImageSkia());
  } else if (is_win8) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    set_menu_marker(
        rb->GetImageNamed(IDR_AVATAR_METRO_BUTTON_DROPARROW).ToImageSkia());
  } else {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_PRESSED);

    SetBorder(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    set_menu_marker(
        rb->GetImageNamed(IDR_AVATAR_GLASS_BUTTON_DROPARROW).ToImageSkia());
  }

  g_browser_process->profile_manager()->GetProfileInfoCache().AddObserver(this);

  // Subscribe to authentication error changes so that the avatar button
  // can update itself.
  SigninErrorController* error =
      ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile())->
          signin_error_controller();
  error->AddObserver(this);
  OnErrorChanged();

  SchedulePaint();
}

NewAvatarButton::~NewAvatarButton() {
  g_browser_process->profile_manager()->
      GetProfileInfoCache().RemoveObserver(this);
  SigninErrorController* error =
      ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile())->
          signin_error_controller();
  error->RemoveObserver(this);
}

void NewAvatarButton::OnPaintText(gfx::Canvas* canvas, PaintButtonMode mode) {
  // Get text bounds, and then adjust for the top and RTL languages.
  gfx::Rect rect = GetTextBounds();
  rect.Offset(0, -rect.y());
  if (rect.width() > 0)
    rect.set_x(GetMirroredXForRect(rect));

  canvas->DrawStringRectWithHalo(
      text(),
      gfx::FontList(),
      SK_ColorWHITE,
      SK_ColorDKGRAY,
      rect,
      gfx::Canvas::NO_SUBPIXEL_RENDERING);
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

void NewAvatarButton::OnErrorChanged() {
  gfx::ImageSkia icon;

  // If there is an error, show an warning icon.
  SigninErrorController* error =
      ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile())->
          signin_error_controller();
  if (error->HasError()) {
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    icon = *rb->GetImageNamed(IDR_WARNING).ToImageSkia();
  }

  SetIcon(icon);
  UpdateAvatarButtonAndRelayoutParent();
}

void NewAvatarButton::UpdateAvatarButtonAndRelayoutParent() {
  // We want the button to resize if the new text is shorter.
  SetText(GetElidedText(
      profiles::GetAvatarNameForProfile(browser_->profile())));
  ClearMaxTextSize();

  // Because the width of the button might have changed, the parent browser
  // frame needs to recalculate the button bounds and redraw it.
  if (parent())
    parent()->Layout();
}
