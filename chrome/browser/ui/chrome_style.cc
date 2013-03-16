// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_style.h"

#include "chrome/browser/themes/theme_properties.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace chrome_style {

int GetCloseButtonSize() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const SkBitmap* image = rb.GetNativeImageNamed(IDR_CLOSE_DIALOG).ToSkBitmap();
  DCHECK_EQ(image->width(), image->height());
  return image->width();
}

SkColor GetBackgroundColor() {
  return ThemeProperties::GetDefaultColor(
      ThemeProperties::COLOR_CONTROL_BACKGROUND);
}

SkColor GetLinkColor() {
  return SkColorSetRGB(0x11, 0x55, 0xCC);
}

}  // namespace chrome_style
