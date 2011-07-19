// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DOWNLOAD_BACKGROUND_THEME_H_
#define CHROME_BROWSER_UI_COCOA_DOWNLOAD_BACKGROUND_THEME_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "ui/base/theme_provider.h"

class BackgroundTheme : public ui::ThemeProvider {
public:
  BackgroundTheme(ui::ThemeProvider* provider);
  virtual ~BackgroundTheme();

  virtual void Init(Profile* profile) {}
  virtual SkBitmap* GetBitmapNamed(int id) const;
  virtual SkColor GetColor(int id) const;
  virtual bool GetDisplayProperty(int id, int* result) const;
  virtual bool ShouldUseNativeFrame() const;
  virtual bool HasCustomImage(int id) const;
  virtual RefCountedMemory* GetRawData(int id) const;
  virtual NSImage* GetNSImageNamed(int id, bool allow_default) const;
  virtual NSColor* GetNSImageColorNamed(int id, bool allow_default) const;
  virtual NSColor* GetNSColor(int id, bool allow_default) const;
  virtual NSColor* GetNSColorTint(int id, bool allow_default) const;
  virtual NSGradient* GetNSGradient(int id) const;

private:
  ui::ThemeProvider* provider_;
  scoped_nsobject<NSGradient> buttonGradient_;
  scoped_nsobject<NSGradient> buttonPressedGradient_;
  scoped_nsobject<NSColor> borderColor_;
};

#endif  // CHROME_BROWSER_UI_COCOA_DOWNLOAD_BACKGROUND_THEME_H_
