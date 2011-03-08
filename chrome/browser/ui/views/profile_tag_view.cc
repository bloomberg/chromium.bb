// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profile_tag_view.h"

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/profile_menu_button.h"
#include "grit/theme_resources.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skbitmap_operations.h"
#include "views/widget/widget.h"

namespace {
// Colors for primary profile. TODO(mirandac): add colors for multi-profile.
color_utils::HSL hsl_active_shift = { 0.594, 0.5, 0.5 };
}

namespace views {

ProfileTagView::ProfileTagView(BrowserFrame* frame,
                               views::ProfileMenuButton* profile_menu_button)
    : profile_tag_bitmaps_created_(false),
      frame_(frame),
      profile_menu_button_(profile_menu_button) {
}

void ProfileTagView::OnPaint(gfx::Canvas* canvas) {
  CreateProfileTagBitmaps();

  // The tag image consists of a right and left edge, and a center section that
  // scales to fit the length of the user's name. We can't just scale the whole
  // image, or the left and right edges would be distorted.
  int tag_width = profile_menu_button_->GetPreferredSize().width();
  int center_tag_width = tag_width - active_profile_tag_left_.width() -
      active_profile_tag_right_.width();
  int tag_x = frame_->GetMinimizeButtonOffset() - tag_width -
      views::ProfileMenuButton::kProfileTagHorizontalSpacing;

  bool is_active = GetWidget()->IsActive();
  SkBitmap* profile_tag_left = is_active ? &active_profile_tag_left_ :
                                           &inactive_profile_tag_left_;
  SkBitmap* profile_tag_center = is_active ? &active_profile_tag_center_ :
                                             &inactive_profile_tag_center_;
  SkBitmap* profile_tag_right = is_active ? &active_profile_tag_right_ :
                                            &inactive_profile_tag_right_;

  canvas->DrawBitmapInt(*profile_tag_left, 0, 0);
  canvas->DrawBitmapInt(*profile_tag_center, 0, 0,
                        profile_tag_center->width(),
                        profile_tag_center->height(),
                        profile_tag_left->width(), 0,
                        center_tag_width,
                        profile_tag_center->height(), true);
  canvas->DrawBitmapInt(*profile_tag_right,
      profile_tag_left->width() + center_tag_width, 0);
}

void ProfileTagView::CreateProfileTagBitmaps() {
  // Lazily create profile tag bitmaps on first display.
  // TODO(mirandac): Cache these per profile, instead of creating every time.
  if (profile_tag_bitmaps_created_)
    return;
  profile_tag_bitmaps_created_ = true;

  ui::ThemeProvider* theme_provider = frame_->GetThemeProviderForFrame();
  SkBitmap* profile_tag_center = theme_provider->GetBitmapNamed(
      IDR_PROFILE_TAG_CENTER);
  SkBitmap* profile_tag_left = theme_provider->GetBitmapNamed(
      IDR_PROFILE_TAG_LEFT);
  SkBitmap* profile_tag_right = theme_provider->GetBitmapNamed(
      IDR_PROFILE_TAG_RIGHT);
  inactive_profile_tag_center_ = *theme_provider->GetBitmapNamed(
      IDR_PROFILE_TAG_INACTIVE_CENTER);
  inactive_profile_tag_left_ = *theme_provider->GetBitmapNamed(
      IDR_PROFILE_TAG_INACTIVE_LEFT);
  inactive_profile_tag_right_ = *theme_provider->GetBitmapNamed(
      IDR_PROFILE_TAG_INACTIVE_RIGHT);

  // Color active bitmap according to profile. TODO(mirandac): add theming
  // and multi-profile color schemes.
  active_profile_tag_center_ = SkBitmapOperations::CreateHSLShiftedBitmap(
      *profile_tag_center, hsl_active_shift);
  active_profile_tag_left_ = SkBitmapOperations::CreateHSLShiftedBitmap(
      *profile_tag_left, hsl_active_shift);
  active_profile_tag_right_ = SkBitmapOperations::CreateHSLShiftedBitmap(
      *profile_tag_right, hsl_active_shift);
}

}  // namespace views
