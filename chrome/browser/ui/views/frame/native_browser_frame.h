// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
#pragma once

namespace gfx {
class Rect;
}

namespace ui {
class ThemeProvider;
}

namespace views {
class NativeWindow;
class View;
}

class NativeBrowserFrame {
 public:
  virtual ~NativeBrowserFrame() {}

  virtual views::NativeWindow* AsNativeWindow() = 0;
  virtual const views::NativeWindow* AsNativeWindow() const = 0;

 protected:
  friend class BrowserFrame;

  // BrowserFrame pass-thrus ---------------------------------------------------
  // See browser_frame.h for documentation:
  virtual int GetMinimizeButtonOffset() const = 0;
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const = 0;
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const = 0;
  virtual void UpdateThrobber(bool running) = 0;
  virtual ui::ThemeProvider* GetThemeProviderForFrame() const = 0;
  virtual bool AlwaysUseNativeFrame() const = 0;
  virtual views::View* GetFrameView() const = 0;
  virtual void TabStripDisplayModeChanged() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
