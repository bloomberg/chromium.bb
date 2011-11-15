// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_GTK_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "views/widget/native_widget_gtk.h"

class BrowserFrameGtk : public views::NativeWidgetGtk,
                        public NativeBrowserFrame {
 public:
  // Normally you will create this class by calling BrowserFrame::Create.
  // Init must be called before using this class, which Create will do for you.
  BrowserFrameGtk(BrowserFrame* browser_frame, BrowserView* browser_view);
  virtual ~BrowserFrameGtk();

 protected:
  // Overridden from NativeBrowserFrame:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const views::NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual int GetMinimizeButtonOffset() const OVERRIDE;
  virtual void TabStripDisplayModeChanged() OVERRIDE;

  // Overridden from views::NativeWidgetGtk:
  virtual gboolean OnWindowStateEvent(GtkWidget* widget,
                                      GdkEventWindowState* event) OVERRIDE;
  virtual gboolean OnConfigureEvent(GtkWidget* widget,
                                    GdkEventConfigure* event) OVERRIDE;

 private:
  NativeBrowserFrameDelegate* delegate_;

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameGtk);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_GTK_H_
