// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_BALLOON_VIEW_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_BALLOON_VIEW_HOST_H_
#pragma once

#include "chrome/browser/notifications/balloon_host.h"
#include "ui/views/controls/native/native_view_host.h"

// BalloonViewHost class is a delegate to the renderer host for the HTML
// notification.  When initialized it creates a new RenderViewHost and loads
// the contents of the toast into it.  It also handles links within the toast,
// loading them into a new tab.
class BalloonViewHost : public BalloonHost {
 public:
  explicit BalloonViewHost(Balloon* balloon);

  virtual ~BalloonViewHost();

  void SetPreferredSize(const gfx::Size& size) {
    native_host_->SetPreferredSize(size);
  }

  // Accessors.
  views::View* view() {
    return native_host_;
  }

  gfx::NativeView native_view() const {
    return native_host_->native_view();
  }

  // Initialize the view, parented to |parent|, and show it.
  void Init(gfx::NativeView parent);

 private:
  // The views-specific host view. Pointer owned by the views hierarchy.
  views::NativeViewHost* native_host_;

  // The handle to the parent view.
  gfx::NativeView parent_native_view_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewHost);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_BALLOON_VIEW_HOST_H_
