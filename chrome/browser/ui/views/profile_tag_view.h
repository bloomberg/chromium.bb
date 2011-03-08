// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILE_TAG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILE_TAG_VIEW_H_
#pragma once

#include "third_party/skia/include/core/SkBitmap.h"
#include "views/view.h"

class BrowserFrame;

namespace gfx {
class Canvas;
}

namespace views {

class ProfileMenuButton;

// ProfileTag
//
// Displays the tinted button image underneath the ProfileMenuButton.

class ProfileTagView : public View {
 public:
  // Height of profile tag.
  static const int kProfileTagHeight = 20;

  ProfileTagView(BrowserFrame* frame,
                 views::ProfileMenuButton* profile_menu_button);
  ~ProfileTagView() {}

  // Paint the profile tag background image on the given canvas.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  // Create the bitmaps to be displayed on the frame behind the profile button.
  void CreateProfileTagBitmaps();

  // True if the bitmaps to display the profile tag have been created.
  bool profile_tag_bitmaps_created_;

  // Bitmaps for the profile tag in active and inactive states.
  SkBitmap active_profile_tag_center_;
  SkBitmap active_profile_tag_left_;
  SkBitmap active_profile_tag_right_;
  SkBitmap inactive_profile_tag_center_;
  SkBitmap inactive_profile_tag_left_;
  SkBitmap inactive_profile_tag_right_;

  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The button to be displayed above this view.
  views::ProfileMenuButton* profile_menu_button_;

  DISALLOW_COPY_AND_ASSIGN(ProfileTagView);
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILE_TAG_VIEW_H_

