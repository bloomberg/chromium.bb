// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profile_tag_view.h"

#include "chrome/browser/themes/theme_service.h"
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

ProfileTagView::ProfileTagView(BrowserFrame* frame,
                               ProfileMenuButton* profile_menu_button)
    : profile_tag_bitmaps_created_(false),
      frame_(frame),
      profile_menu_button_(profile_menu_button) {
}

ProfileTagView::~ProfileTagView() {
}

void ProfileTagView::OnPaint(gfx::Canvas* canvas) {
  CreateProfileTagBitmaps();

  // The tag image consists of a right and left edge, and a center section that
  // scales to fit the length of the user's name. We can't just scale the whole
  // image, or the left and right edges would be distorted.
  int tag_width = profile_menu_button_->GetPreferredSize().width();
  int center_tag_width = tag_width - active_profile_tag_left_.width() -
      active_profile_tag_right_.width();

  bool use_active = GetWidget()->IsActive() && is_signed_in_;
  SkBitmap* profile_tag_left = use_active ? &active_profile_tag_left_ :
                                            &inactive_profile_tag_left_;
  SkBitmap* profile_tag_center = use_active ? &active_profile_tag_center_ :
                                              &inactive_profile_tag_center_;
  SkBitmap* profile_tag_right = use_active ? &active_profile_tag_right_ :
                                             &inactive_profile_tag_right_;

  if (!active_profile_tag_left_background_.empty()) {
    canvas->DrawBitmapInt(active_profile_tag_left_background_, 0, 0);
    canvas->DrawBitmapInt(active_profile_tag_center_background_, 0, 0,
                          profile_tag_center->width(),
                          profile_tag_center->height(),
                          profile_tag_left->width(), 0,
                          center_tag_width,
                          profile_tag_center->height(), true);
    canvas->DrawBitmapInt(active_profile_tag_right_background_,
        profile_tag_left->width() + center_tag_width, 0);
  }

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

  ui::ThemeProvider* theme_provider = frame_->GetThemeProvider();
  bool aero = theme_provider->ShouldUseNativeFrame();
  SkBitmap* profile_tag_center = aero ?
      theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_CENTER_AERO) :
      theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_CENTER_THEMED);
  SkBitmap* profile_tag_left = aero ?
      theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_LEFT_AERO) :
      theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_LEFT_THEMED);
  SkBitmap* profile_tag_right = aero ?
      theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_RIGHT_AERO) :
      theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_RIGHT_THEMED);
  inactive_profile_tag_center_ = aero ?
      *theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_INACTIVE_CENTER_AERO) :
      *theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_CENTER_THEMED);
  inactive_profile_tag_left_ = aero ?
      *theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_INACTIVE_LEFT_AERO) :
      *theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_LEFT_THEMED);
  inactive_profile_tag_right_ = aero ?
      *theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_INACTIVE_RIGHT_AERO) :
      *theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_RIGHT_THEMED);

  // Color if we're using the Aero theme; otherwise the tag will be given by
  // the window controls background from the theme.
  if (theme_provider->ShouldUseNativeFrame()) {
    active_profile_tag_center_ = SkBitmapOperations::CreateHSLShiftedBitmap(
        *profile_tag_center, hsl_active_shift);
    active_profile_tag_left_ = SkBitmapOperations::CreateHSLShiftedBitmap(
        *profile_tag_left, hsl_active_shift);
    active_profile_tag_right_ = SkBitmapOperations::CreateHSLShiftedBitmap(
        *profile_tag_right, hsl_active_shift);

    // No backgrounds used in Aero theme.
    active_profile_tag_center_background_.reset();
    active_profile_tag_left_background_.reset();
    active_profile_tag_center_background_.reset();
  } else {
    active_profile_tag_center_ = *profile_tag_center;
    active_profile_tag_left_ = *profile_tag_left;
    active_profile_tag_right_ = *profile_tag_right;

    SkBitmap* background = theme_provider->GetBitmapNamed(
        IDR_THEME_WINDOW_CONTROL_BACKGROUND);
    if (!background) {
      active_profile_tag_center_background_.reset();
      active_profile_tag_left_background_.reset();
      active_profile_tag_center_background_.reset();
    } else {
      active_profile_tag_center_background_ =
          SkBitmapOperations::CreateButtonBackground(
              theme_provider->GetColor(ThemeService::COLOR_BUTTON_BACKGROUND),
              *background,
              *(theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_CENTER_MASK)));
      active_profile_tag_left_background_ =
          SkBitmapOperations::CreateButtonBackground(
              theme_provider->GetColor(ThemeService::COLOR_BUTTON_BACKGROUND),
              *background,
              *(theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_LEFT_MASK)));
      active_profile_tag_right_background_ =
          SkBitmapOperations::CreateButtonBackground(
              theme_provider->GetColor(ThemeService::COLOR_BUTTON_BACKGROUND),
              *background,
              *(theme_provider->GetBitmapNamed(IDR_PROFILE_TAG_RIGHT_MASK)));
    }
  }
}
