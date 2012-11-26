// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/constrained_window.h"

#include "base/logging.h"
#include "chrome/browser/themes/theme_service.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

// static
int ConstrainedWindow::GetCloseButtonSize() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  const SkBitmap* bitmap =
      bundle.GetNativeImageNamed(IDR_WEB_UI_CLOSE).ToSkBitmap();
  DCHECK_EQ(bitmap->width(), bitmap->height());
  return bitmap->width();
}

// static
SkColor ConstrainedWindow::GetBackgroundColor() {
  return ThemeService::GetDefaultColor(ThemeService::COLOR_CONTROL_BACKGROUND);
}

// static
SkColor ConstrainedWindow::GetLinkColor() {
  return SkColorSetRGB(0x11, 0x55, 0xCC);
}

// static
SkColor ConstrainedWindow::GetSeparatorColor() {
  return SkColorSetRGB(0xE0, 0xE0, 0xE0);
}

void ConstrainedWindow::FocusConstrainedWindow() {
}

void ConstrainedWindow::PulseConstrainedWindow() {
}

bool ConstrainedWindow::CanShowConstrainedWindow() {
  return true;
}

gfx::NativeWindow ConstrainedWindow::GetNativeWindow() {
  NOTREACHED();
  return NULL;
}
