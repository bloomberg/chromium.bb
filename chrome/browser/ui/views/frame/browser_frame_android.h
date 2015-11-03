// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_ANDROID_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_ANDROID_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/views/widget/native_widget_aura.h"

class BrowserFrame;
class BrowserView;

////////////////////////////////////////////////////////////////////////////////
//  BrowserFrameAndroid
//
//  BrowserFrameAndroid is a NativeWidgetAura subclass that provides the window
//  frame for the Chrome browser window.
//
class BrowserFrameAndroid : public views::NativeWidgetAura,
                            public NativeBrowserFrame {
 public:
  static const char kWindowName[];

  BrowserFrameAndroid(BrowserFrame* browser_frame, BrowserView* browser_view);

  BrowserView* browser_view() const { return browser_view_; }

  // Must be called before the browser frame is created.
  static void SetHost(aura::WindowTreeHostPlatform* host);

 protected:
  // Overridden from views::NativeWidgetAura:
  void OnWindowDestroying(aura::Window* window) override;

  // Overridden from NativeBrowserFrame:
  views::Widget::InitParams GetWidgetParams() override;
  bool UseCustomFrame() const override;
  bool UsesNativeSystemMenu() const override;
  int GetMinimizeButtonOffset() const override;
  bool ShouldSaveWindowPlacement() const override;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const override;

  ~BrowserFrameAndroid() override;

 private:
  class WindowPropertyWatcher;

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameAndroid);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_ANDROID_H_
