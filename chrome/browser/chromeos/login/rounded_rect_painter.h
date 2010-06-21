// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ROUNDED_RECT_PAINTER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ROUNDED_RECT_PAINTER_H_

#include "third_party/skia/include/core/SkColor.h"

namespace views {
class Border;
class Painter;
}  // namespace views

namespace chromeos {

struct BorderDefinition {
  int padding;
  SkColor padding_color;
  int shadow;
  SkColor shadow_color;
  int corner_radius;
  SkColor top_color;
  SkColor bottom_color;

  static const BorderDefinition kScreenBorder;
};

// Creates painter to paint view background with parameters specified.
views::Painter* CreateWizardPainter(const BorderDefinition* const border);
// Creates border to provide insets. Should be used together with WizardPainter
// that actually draws both border and background.
views::Border* CreateWizardBorder(const BorderDefinition* const border);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ROUNDED_RECT_PAINTER_H_
