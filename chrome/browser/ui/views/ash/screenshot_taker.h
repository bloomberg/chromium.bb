// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_SCREENSHOT_TAKER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_SCREENSHOT_TAKER_H_
#pragma once

#include "ash/screenshot_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/compositor/layer.h"

namespace aura {
class Window;
}  // namespace aura

class ScreenshotTaker : public ash::ScreenshotDelegate {
 public:
  ScreenshotTaker();
  virtual ~ScreenshotTaker();

  // Overridden from ash::ScreenshotDelegate:
  virtual void HandleTakeScreenshot(aura::Window* window) OVERRIDE;
  virtual void HandleTakePartialScreenshot(
      aura::Window* window, const gfx::Rect& rect) OVERRIDE;

 private:
  // Flashes the screen to provide visual feedback that a screenshot has
  // been taken.
  void DisplayVisualFeedback(const gfx::Rect& rect);

  // Closes the visual feedback layer.
  void CloseVisualFeedbackLayer();

  scoped_ptr<ui::Layer> visual_feedback_layer_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTaker);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_SCREENSHOT_TAKER_H_
