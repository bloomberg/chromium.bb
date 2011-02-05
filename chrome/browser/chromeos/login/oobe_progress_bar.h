// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_PROGRESS_BAR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_PROGRESS_BAR_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "ui/gfx/font.h"
#include "views/view.h"

class SkBitmap;

namespace chromeos {

// Special purpose progress bar with labeled steps that is used to show user's
// progress in OOBE process.
class OobeProgressBar : public views::View {
 public:
  // Construct progress bar with given labels as steps.
  // |steps| is a vector of string IDs from resources.
  explicit OobeProgressBar(const std::vector<int>& steps);

  // Overridden from View:
  virtual void Paint(gfx::Canvas* canvas);

  // Set the current step for the progress bar. Must be one of the steps
  // passed in the constructor.
  void SetStep(int step);

 protected:
  // Overridden from View:
  virtual void OnLocaleChanged();

 private:
  static void InitClass();

  // Graphics.
  static SkBitmap* dot_current_;
  static SkBitmap* dot_empty_;
  static SkBitmap* dot_filled_;
  static SkBitmap* line_;
  static SkBitmap* line_left_;
  static SkBitmap* line_right_;

  gfx::Font font_;

  // Unique ids for progress bar steps. The order defines how the steps are
  // enumerated on screen.
  std::vector<int> steps_;

  //  Index of the current step.
  size_t progress_;

  DISALLOW_COPY_AND_ASSIGN(OobeProgressBar);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_PROGRESS_BAR_H_
