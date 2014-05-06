// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_ASH_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "ui/views/widget/native_widget_aura.h"

class BrowserFrame;
class BrowserView;

////////////////////////////////////////////////////////////////////////////////
//  BrowserFrameAsh
//
//  BrowserFrameAsh is a NativeWidgetAura subclass that provides the window
//  frame for the Chrome browser window.
//
class BrowserFrameAsh : public views::NativeWidgetAura,
                        public NativeBrowserFrame {
 public:
  static const char kWindowName[];

  BrowserFrameAsh(BrowserFrame* browser_frame, BrowserView* browser_view);

  BrowserView* browser_view() const { return browser_view_; }

 protected:
  // Overridden from views::NativeWidgetAura:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE;

  // Overridden from NativeBrowserFrame:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const views::NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual bool UsesNativeSystemMenu() const OVERRIDE;
  virtual int GetMinimizeButtonOffset() const OVERRIDE;
  virtual bool ShouldSaveWindowPlacement() const OVERRIDE;
  virtual void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;


  virtual ~BrowserFrameAsh();

 private:
  class WindowPropertyWatcher;

  // Set the window into the auto managed mode.
  void SetWindowAutoManaged();

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_ASH_H_
