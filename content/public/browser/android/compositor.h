// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_

#include "base/callback.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace WebKit {
class WebLayer;
}

namespace content {

// An interface to the browser-side compositor.
class Compositor {
 public:
  virtual ~Compositor() {}

  // Performs the global initialization needed before any compositor
  // instance can be used.
  static void Initialize();

  // Creates and returns a compositor instance.
  static Compositor* Create();

  // Attaches the layer tree.
  virtual void SetRootLayer(WebKit::WebLayer* root) = 0;

  // Set the output surface bounds.
  virtual void SetWindowBounds(const gfx::Size& size) = 0;

  // Set the output surface handle which the compositor renders into.
  virtual void SetWindowSurface(ANativeWindow* window) = 0;

  // Attempts to composite and read back the result into the provided buffer.
  // The buffer must be at least window width * height * 4 (RGBA) bytes large.
  // The buffer is not modified if false is returned.
  virtual bool CompositeAndReadback(void *pixels, const gfx::Rect& rect) = 0;

  // Callback to be run after the frame has been drawn. It passes back
  // a synchronization point identifier.
  typedef base::Callback<void(uint32)> SurfacePresentedCallback;

  virtual void OnSurfaceUpdated(const SurfacePresentedCallback& callback) = 0;

 protected:
  Compositor() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
