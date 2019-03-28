// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_SEPARATOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_SEPARATOR_VIEW_H_

#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

class LocationBarSeparatorView : public views::View {
 public:
  LocationBarSeparatorView();

  void SetBackgroundColor(SkColor color) { background_color_ = color; }

  // Show or hide immediately.
  void Show();
  void Hide();

  // Animate in or out.
  void FadeIn();
  void FadeOut();

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  void set_disable_animation_for_test(bool disable_animation_for_test) {
    disable_animation_for_test_ = disable_animation_for_test;
  }

 private:
  void FadeTo(float opacity, base::TimeDelta duration);

  SkColor background_color_ = gfx::kPlaceholderColor;
  bool disable_animation_for_test_ = false;

  DISALLOW_COPY_AND_ASSIGN(LocationBarSeparatorView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_SEPARATOR_VIEW_H_
