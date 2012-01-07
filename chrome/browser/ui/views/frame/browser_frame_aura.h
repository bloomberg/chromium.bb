// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_AURA_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/native_widget_aura.h"

class BrowserFrame;
class BrowserView;

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameAura
//
//  BrowserFrameAura is a NativeWidgetAura subclass that provides the window
//  frame for the Chrome browser window.
//
class BrowserFrameAura : public views::NativeWidgetAura,
                         public NativeBrowserFrame,
                         public aura::WindowObserver {
 public:
  BrowserFrameAura(BrowserFrame* browser_frame, BrowserView* browser_view);
  virtual ~BrowserFrameAura();

  BrowserView* browser_view() const { return browser_view_; }

 protected:
  // Overridden from views::NativeWidgetAura:
  virtual void OnWindowDestroying() OVERRIDE;

  // Overridden from NativeBrowserFrame:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const views::NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual int GetMinimizeButtonOffset() const OVERRIDE;
  virtual void TabStripDisplayModeChanged() OVERRIDE;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const char* key,
                                       void* old) OVERRIDE;
 private:
  class StatusAreaBoundsWatcher;

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  BrowserFrame* browser_frame_;

  scoped_ptr<StatusAreaBoundsWatcher> status_area_watcher_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_AURA_H_
