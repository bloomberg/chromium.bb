// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_SCREENSHOT_TAKER_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_SCREENSHOT_TAKER_H_
#pragma once

#include "ash/screenshot_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

class ScreenshotTaker : public ash::ScreenshotDelegate {
 public:
  ScreenshotTaker();

  virtual void  HandleTakeScreenshot() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenshotTaker);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_SCREENSHOT_TAKER_H_
