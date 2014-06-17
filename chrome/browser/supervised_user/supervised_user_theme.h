// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_THEME_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_THEME_H_

#include "chrome/browser/themes/custom_theme_supplier.h"

namespace gfx {
class Image;
}

// This theme is used as a default theme for supervised users. It uses a light
// blue as frame color, and defines color properties for the avatar label.
class SupervisedUserTheme : public CustomThemeSupplier {
 public:
  SupervisedUserTheme();

  // Overridden from CustomThemeSupplier:
  virtual bool GetColor(int id, SkColor* color) const OVERRIDE;
  virtual gfx::Image GetImageNamed(int id) OVERRIDE;
  virtual bool HasCustomImage(int id) const OVERRIDE;

 private:
  virtual ~SupervisedUserTheme();

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserTheme);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_THEME_H_
