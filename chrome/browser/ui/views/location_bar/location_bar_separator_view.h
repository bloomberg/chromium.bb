// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_SEPARATOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_SEPARATOR_VIEW_H_

#include "base/compiler_specific.h"
#include "ui/gfx/size.h"
#include "ui/views/view.h"

// LocationBarSeparatorView is used by the location bar view to display a
// separator between decorations.
// Internally LocationBarSeparatorView draws a 1-px thick vertical line.
class LocationBarSeparatorView : public views::View {
 public:
  LocationBarSeparatorView();
  virtual ~LocationBarSeparatorView();

  // Default is solid black color.
  void set_separator_color(SkColor color) {
    separator_color_ = color;
  }

 private:
  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  SkColor separator_color_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarSeparatorView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_SEPARATOR_VIEW_H_
