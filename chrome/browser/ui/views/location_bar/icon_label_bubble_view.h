// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
#pragma once

#include <string>

#include "ui/gfx/size.h"
#include "views/painter.h"
#include "views/view.h"

namespace gfx {
class Canvas;
class Font;
}
namespace views {
class ImageView;
class Label;
}

class SkBitmap;

// View used to draw a bubble to the left of the address, containing an icon and
// a label.  We use this as a base for the classes that handle the EV bubble and
// tab-to-search UI.
class IconLabelBubbleView : public views::View {
 public:
  IconLabelBubbleView(const int background_images[],
                      int contained_image,
                      const SkColor& color);
  virtual ~IconLabelBubbleView();

  void SetFont(const gfx::Font& font);
  void SetLabel(const std::wstring& label);
  void SetImage(const SkBitmap& bitmap);
  void SetItemPadding(int padding) { item_padding_ = padding; }

  virtual void OnPaint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

 protected:
  void SetElideInMiddle(bool elide_in_middle);
  gfx::Size GetNonLabelSize();

 private:
  int GetNonLabelWidth();

  // For painting the background.
  views::HorizontalPainter background_painter_;

  // The contents of the bubble.
  views::ImageView* image_;
  views::Label* label_;

  int item_padding_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IconLabelBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
