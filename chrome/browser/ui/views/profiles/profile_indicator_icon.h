// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_INDICATOR_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_INDICATOR_ICON_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/image/image.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

// ProfileIndicatorIcon
//
// A view used to show a profile avatar for teleported windows in CrOS. The icon
// set via SetIcon() will be resized and drawn inside a circle if it's too big
// to fit in the frame.
class ProfileIndicatorIcon : public views::View {
 public:
  ProfileIndicatorIcon();
  ~ProfileIndicatorIcon() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // Sets the image for the avatar button. Rectangular images, as opposed
  // to Chrome avatar icons, will be resized and modified for the title bar.
  void SetIcon(const gfx::Image& icon);

 private:
  gfx::Image base_icon_;
  gfx::ImageSkia modified_icon_;
  int old_height_ = 0;
  DISALLOW_COPY_AND_ASSIGN(ProfileIndicatorIcon);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_INDICATOR_ICON_H_
