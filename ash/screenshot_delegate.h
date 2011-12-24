// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCREENSHOT_DELEGATE_H_
#define ASH_SCREENSHOT_DELEGATE_H_
#pragma once

namespace ash {

// Delegate for taking screenshots.
class ScreenshotDelegate {
 public:
  virtual ~ScreenshotDelegate() {}

  // The actual task of taking a screenshot.  This method is called
  // when the user wants to take a screenshot manually.
  virtual void HandleTakeScreenshot() = 0;
};
}  // namespace ash

#endif  // ASH_SCREENSHOT_DELEGATE_H_
