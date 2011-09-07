// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_AURA_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "views/widget/native_widget_aura.h"

class BrowserView;

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameAura
//
//  BrowserFrameAura is a NativeWidgetAura subclass that provides the window
//  frame for the Chrome browser window.
//
class BrowserFrameAura : public views::NativeWidgetAura,
                         public NativeBrowserFrame {
 public:
  BrowserFrameAura(BrowserFrame* browser_frame, BrowserView* browser_view);
  virtual ~BrowserFrameAura();

  BrowserView* browser_view() const { return browser_view_; }

 protected:
  // Overridden from views::NativeWidgetAura:

  // Overridden from NativeBrowserFrame:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const views::NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual int GetMinimizeButtonOffset() const OVERRIDE;
  virtual void TabStripDisplayModeChanged() OVERRIDE;

 private:
  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  BrowserFrame* browser_frame_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_AURA_H_
