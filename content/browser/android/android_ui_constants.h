// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_ANDROID_UI_CONSTANTS_H_
#define CONTENT_BROWSER_ANDROID_ANDROID_UI_CONSTANTS_H_

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/skia/include/core/SkColor.h"

class AndroidUiConstants {
 public:
  static bool IsFocusRingOutset();
  static base::Optional<float> GetMinimumStrokeWidthForFocusRing();
  static base::Optional<SkColor> GetFocusRingColor();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AndroidUiConstants);
};

#endif  // CONTENT_BROWSER_ANDROID_ANDROID_UI_CONSTANTS_H_
