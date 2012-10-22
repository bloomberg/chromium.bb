// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_

#include "base/callback.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

namespace gfx {
class JavaBitmap;
}

namespace WebKit {
class WebLayer;
}

namespace content {

// An interface to the browser-side compositor.
class Compositor {
 public:
  class Client {
   public:
    // Tells the client that it should schedule a composite.
    virtual void ScheduleComposite() = 0;

    // The compositor has completed swapping a frame.
    virtual void OnSwapBuffersCompleted() {}
  };

  virtual ~Compositor() {}

  // Performs the global initialization needed before any compositor
  // instance can be used.
  static void Initialize();

  // Creates and returns a compositor instance.
  static Compositor* Create(Client* client);

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

  // Composite immediately. Used in single-threaded mode.
  virtual void Composite() = 0;

  // Generates an OpenGL texture and returns a texture handle.  May return 0
  // if the current context is lost.
  virtual WebKit::WebGLId GenerateTexture(gfx::JavaBitmap& bitmap) = 0;

  // Generates an OpenGL compressed texture and returns a texture handle.  May
  // return 0 if the current context is lost.
  virtual WebKit::WebGLId GenerateCompressedTexture(gfx::Size& size,
                                                    int data_size,
                                                    void* data) = 0;

  // Deletes an OpenGL texture.
  virtual void DeleteTexture(WebKit::WebGLId texture_id) = 0;

 protected:
  Compositor() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
