// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEWS_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "views/widget/native_widget_views.h"

class BrowserView;

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameViews
//
//  BrowserFrameViews is a NativeWidgetViews subclass that provides the window
//  frame for the Chrome browser window.
//
class BrowserFrameViews : public views::NativeWidgetViews,
                          public NativeBrowserFrame {
 public:
  BrowserFrameViews(BrowserFrame* browser_frame, BrowserView* browser_view);
  virtual ~BrowserFrameViews();

  BrowserView* browser_view() const { return browser_view_; }

 protected:
  // Overridden from NativeBrowserFrame:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const views::NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual int GetMinimizeButtonOffset() const OVERRIDE;
  virtual void TabStripDisplayModeChanged() OVERRIDE;

 private:
  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  BrowserFrame* browser_frame_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEWS_H_
