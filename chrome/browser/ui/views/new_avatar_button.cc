// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/new_avatar_button.h"

#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace {

// Text padding within the button border.
const int kInset = 10;

views::TextButtonDefaultBorder* CreateBorder(const int normal_image_set[],
                                             const int hot_image_set[],
                                             const int pushed_image_set[]) {
  views::TextButtonDefaultBorder* border = new views::TextButtonDefaultBorder();

  border->SetInsets(gfx::Insets(kInset, kInset, kInset, kInset));
  border->set_normal_painter(
      views::Painter::CreateImageGridPainter(normal_image_set));
  border->set_hot_painter(
      views::Painter::CreateImageGridPainter(hot_image_set));
  border->set_pushed_painter(
      views::Painter::CreateImageGridPainter(pushed_image_set));

  return border;
}

}  // namespace

NewAvatarButton::NewAvatarButton(
    views::ButtonListener* listener,
    const string16& profile_name,
    AvatarButtonStyle button_style,
    Browser* browser)
    : MenuButton(listener, profile_name, NULL, true),
      browser_(browser) {
  set_animate_on_state_change(false);

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  SetFont(rb->GetFont(ui::ResourceBundle::BaseFont));

  bool is_win8 = false;
#if defined(OS_WIN)
  is_win8 = base::win::GetVersion() >= base::win::VERSION_WIN8;
#endif

  if (button_style == THEMED_BUTTON) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_THEMED_BUTTON_PRESSED);

    set_border(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    set_menu_marker(
        rb->GetImageNamed(IDR_AVATAR_THEMED_BUTTON_DROPARROW).ToImageSkia());
  } else if (is_win8) {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_METRO_BUTTON_PRESSED);

    set_border(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    set_menu_marker(
        rb->GetImageNamed(IDR_AVATAR_METRO_BUTTON_DROPARROW).ToImageSkia());
  } else {
    const int kNormalImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_NORMAL);
    const int kHotImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_HOVER);
    const int kPushedImageSet[] = IMAGE_GRID(IDR_AVATAR_GLASS_BUTTON_PRESSED);

    set_border(CreateBorder(kNormalImageSet, kHotImageSet, kPushedImageSet));
    set_menu_marker(
        rb->GetImageNamed(IDR_AVATAR_GLASS_BUTTON_DROPARROW).ToImageSkia());
  }

  avatar_menu_.reset(new AvatarMenu(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      this,
      browser_));
  avatar_menu_->RebuildMenu();

  SchedulePaint();
}

NewAvatarButton::~NewAvatarButton() {
}

void NewAvatarButton::OnPaint(gfx::Canvas* canvas) {
  // From TextButton::PaintButton, draw everything but the text.
  OnPaintBackground(canvas);
  OnPaintBorder(canvas);
  OnPaintFocusBorder(canvas);

  gfx::Rect rect;
  // In RTL languages the marker gets drawn leftmost, so account for its offset.
  if (base::i18n::IsRTL())
    rect = gfx::Rect(-kInset, 0, size().width(), size().height());
  else
    rect = gfx::Rect(kInset, 0, size().width(), size().height());

  canvas->DrawStringRectWithHalo(
      text(),
      gfx::FontList(ui::ResourceBundle::GetSharedInstance().GetFont(
          ui::ResourceBundle::BaseFont)),
      SK_ColorWHITE,
      SK_ColorDKGRAY,
      rect,
      gfx::Canvas::NO_SUBPIXEL_RENDERING);

  // From MenuButton::PaintButton, paint the marker
  PaintMenuMarker(canvas);
}

void NewAvatarButton::OnAvatarMenuChanged(AvatarMenu* avatar_menu) {
  SetText(profiles::GetActiveProfileDisplayName(browser_));
  // We need to redraw the entire button because the width might have changed.
  SchedulePaint();
}
