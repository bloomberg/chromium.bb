// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SCREENSHOT_TAKER_H_
#define CHROME_BROWSER_UI_ASH_SCREENSHOT_TAKER_H_

#include "ash/screenshot_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
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
  virtual void HandleTakePartialScreenshot(aura::Window* window,
                                           const gfx::Rect& rect) OVERRIDE;
  virtual bool CanTakeScreenshot() OVERRIDE;

 private:
  // Flashes the screen to provide visual feedback that a screenshot has
  // been taken.
  void DisplayVisualFeedback(const gfx::Rect& rect);

  // Closes the visual feedback layer.
  void CloseVisualFeedbackLayer();

  // The timestamp when the screenshot task was issued last time.
  base::Time last_screenshot_timestamp_;

  // The flashing effect of the screen for the visual feedback when taking a
  // screenshot.
  scoped_ptr<ui::Layer> visual_feedback_layer_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTaker);
};

#endif  // CHROME_BROWSER_UI_ASH_SCREENSHOT_TAKER_H_
