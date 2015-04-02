// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_

#include "ui/views/window/native_frame_view.h"

class Widget;

// Provides metrics consistent with a native frame on Mac. The actual frame is
// drawn by NSWindow.
class NativeAppWindowFrameViewMac : public views::NativeFrameView {
 public:
  explicit NativeAppWindowFrameViewMac(views::Widget* frame);
  ~NativeAppWindowFrameViewMac() override;

  // NonClientFrameView:
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowFrameViewMac);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_
