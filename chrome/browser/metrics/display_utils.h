// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DISPLAY_UTILS_H_
#define CHROME_BROWSER_METRICS_DISPLAY_UTILS_H_
#pragma once

#include "base/basictypes.h"

class DisplayUtils {
 public:
  // Returns the pixel dimensions of the primary display via the
  // width and height parameters.
  static void GetPrimaryDisplayDimensions(int* width, int* height);

  // Returns the number of displays.
  static int GetDisplayCount();

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayUtils);
};

#endif  // CHROME_BROWSER_METRICS_DISPLAY_UTILS_H_
