// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_10_CAPTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_10_CAPTION_BUTTON_H_

#include "chrome/browser/ui/view_ids.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/custom_button.h"

class GlassBrowserFrameView;

class Windows10CaptionButton : public views::CustomButton {
 public:
  Windows10CaptionButton(GlassBrowserFrameView* frame_view, ViewID button_type);

  // views::CustomButton:
  gfx::Size GetPreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // The base color to use for the button symbols and background blending. Uses
  // the more readable of black and white.
  SkColor GetBaseColor() const;

  // Paints the background of the button (the semi-transparent rectangle that
  // appears when you hover or press the button).
  void PaintBackground(gfx::Canvas* canvas);

  // Paints the minimize/maximize/restore/close icon for the button.
  void PaintSymbol(gfx::Canvas* canvas);

  GlassBrowserFrameView* frame_view_;
  ViewID button_type_;

  DISALLOW_COPY_AND_ASSIGN(Windows10CaptionButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_10_CAPTION_BUTTON_H_
