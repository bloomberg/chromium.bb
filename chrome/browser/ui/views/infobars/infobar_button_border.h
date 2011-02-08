// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BUTTON_BORDER_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BUTTON_BORDER_H_
#pragma once

#include "views/border.h"

class SkBitmap;
namespace gfx {
class Canvas;
}
namespace views {
class View;
}

// A TextButtonBorder that is dark and also paints the button frame in the
// normal state.
class InfoBarButtonBorder : public views::Border {
 public:
  InfoBarButtonBorder();

 private:
  virtual ~InfoBarButtonBorder();

  // views::Border:
  virtual void GetInsets(gfx::Insets* insets) const;
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) const;

  struct MBBImageSet {
    SkBitmap* top_left;
    SkBitmap* top;
    SkBitmap* top_right;
    SkBitmap* left;
    SkBitmap* center;
    SkBitmap* right;
    SkBitmap* bottom_left;
    SkBitmap* bottom;
    SkBitmap* bottom_right;
  };

  MBBImageSet normal_set_;
  MBBImageSet hot_set_;
  MBBImageSet pushed_set_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarButtonBorder);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BUTTON_BORDER_H_
