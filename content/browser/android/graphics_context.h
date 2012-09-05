// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_GRAPHICS_CONTEXT_H_
#define CONTENT_BROWSER_ANDROID_GRAPHICS_CONTEXT_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"

class WebGraphicsContext3DCommandBufferImpl;
struct ANativeWindow;

namespace WebKit {
class WebGraphicsContext3D;
}

namespace content {

class GraphicsContext {
 public:
  ~GraphicsContext();

  // Create and returns a graphics context on the caller's thread and
  // also creates an image transport surface associated with the given
  // parent window.
  static GraphicsContext* CreateForUI(ANativeWindow* ui_window);

  uint32 InsertSyncPoint();
  int GetSurfaceID();

 private:
  GraphicsContext(WebGraphicsContext3DCommandBufferImpl* context,
                  int surface_id,
                  ANativeWindow* window,
                  int texture_id1,
                  int texture_id2);

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context_;
  int surface_id_;
  ANativeWindow* window_;
  int texture_id_[2];
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_GRAPHICS_CONTEXT_H_
