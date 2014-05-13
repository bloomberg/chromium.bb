// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DOWNLOAD_BACKGROUND_THEME_H_
#define CHROME_BROWSER_UI_COCOA_DOWNLOAD_BACKGROUND_THEME_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "ui/base/theme_provider.h"

class BackgroundTheme : public ui::ThemeProvider {
 public:
  BackgroundTheme(ui::ThemeProvider* provider);
  virtual ~BackgroundTheme();

  // Overridden from ui::ThemeProvider:
  virtual bool UsingSystemTheme() const OVERRIDE;
  virtual gfx::ImageSkia* GetImageSkiaNamed(int id) const OVERRIDE;
  virtual SkColor GetColor(int id) const OVERRIDE;
  virtual int GetDisplayProperty(int id) const OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual bool HasCustomImage(int id) const OVERRIDE;
  virtual base::RefCountedMemory* GetRawData(
      int id,
      ui::ScaleFactor scale_factor) const OVERRIDE;
  virtual NSImage* GetNSImageNamed(int id) const OVERRIDE;
  virtual NSColor* GetNSImageColorNamed(int id) const OVERRIDE;
  virtual NSColor* GetNSColor(int id) const OVERRIDE;
  virtual NSColor* GetNSColorTint(int id) const OVERRIDE;
  virtual NSGradient* GetNSGradient(int id) const OVERRIDE;

 private:
  ui::ThemeProvider* provider_;
  base::scoped_nsobject<NSGradient> buttonGradient_;
  base::scoped_nsobject<NSGradient> buttonPressedGradient_;
  base::scoped_nsobject<NSColor> borderColor_;
};

#endif  // CHROME_BROWSER_UI_COCOA_DOWNLOAD_BACKGROUND_THEME_H_
