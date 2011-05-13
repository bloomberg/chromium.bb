// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
#pragma once

class BrowserView;

class NativeBrowserFrame {
 public:
  virtual ~NativeBrowserFrame() {}

  // Construct a platform-specific implementation of this interface.
  static NativeBrowserFrame* CreateNativeBrowserFrame(
      BrowserFrame* browser_frame,
      BrowserView* browser_view);

  virtual views::NativeWindow* AsNativeWindow() = 0;
  virtual const views::NativeWindow* AsNativeWindow() const = 0;

 protected:
  friend class BrowserFrame;

  // BrowserFrame pass-thrus ---------------------------------------------------
  // See browser_frame.h for documentation:
  virtual int GetMinimizeButtonOffset() const = 0;
  // TODO(beng): replace with some kind of "framechanged" signal to Window.
  virtual void TabStripDisplayModeChanged() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
