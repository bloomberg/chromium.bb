// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NATIVE_FOCUS_TRACKER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_NATIVE_FOCUS_TRACKER_VIEWS_H_

#include "chrome/browser/ui/native_focus_tracker.h"
#include "ui/views/focus/widget_focus_manager.h"

class Browser;

class NativeFocusTrackerViews : public NativeFocusTracker,
                                public views::WidgetFocusChangeListener {
 public:
  explicit NativeFocusTrackerViews(NativeFocusTrackerHost* host);
  virtual ~NativeFocusTrackerViews();

 private:
  static Browser* GetBrowserForNativeView(gfx::NativeView view);

  // views::WidgetFocusChangeListener::
  virtual void OnNativeFocusChange(gfx::NativeView focused_before,
                                   gfx::NativeView focused_now) OVERRIDE;

  NativeFocusTrackerHost* host_;

  DISALLOW_COPY_AND_ASSIGN(NativeFocusTrackerViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NATIVE_FOCUS_TRACKER_VIEWS_H_
