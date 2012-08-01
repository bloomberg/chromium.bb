// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_GRAPHICS_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_GRAPHICS_CONTEXT_H_

#include "ui/gfx/native_widget_types.h"

struct ANativeWindow;

namespace WebKit {
class WebGraphicsContext3D;
}

namespace content {

class GraphicsContext {
 public:
  virtual ~GraphicsContext() { }

  // Create a UI graphics context that renders to the given surface.
  static GraphicsContext* CreateForUI(ANativeWindow* ui_window);

  virtual WebKit::WebGraphicsContext3D* GetContext3D() = 0;
  virtual uint32 InsertSyncPoint() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_GRAPHICS_CONTEXT_H_
