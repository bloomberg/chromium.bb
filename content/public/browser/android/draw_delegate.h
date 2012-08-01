// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_DRAW_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_DRAW_DELEGATE_H_

#include "base/callback.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace content {

class RenderWidgetHostView;

// This interface facilitates the communication between compositor and
// UI with respect to new frames arriving and acknowledging when they
// have been presented.
class DrawDelegate {
 public:
  static DrawDelegate* GetInstance();
  virtual ~DrawDelegate() { }

  // Callback to be run after the frame has been drawn. It passes back
  // a synchronization point identifier.
  typedef base::Callback<void(uint32)> SurfacePresentedCallback;

  // Notification to the UI that the surface identified by the texture id
  // has been updated.
  typedef base::Callback<void(
      uint64,
      RenderWidgetHostView*,
      const SurfacePresentedCallback&)> SurfaceUpdatedCallback;

  virtual void SetUpdateCallback(const SurfaceUpdatedCallback& callback) = 0;
  virtual void SetBounds(const gfx::Size& size) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_DRAW_DELEGATE_H_
