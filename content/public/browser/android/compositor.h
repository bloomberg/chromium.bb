// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

namespace cc {
class Layer;
}

namespace gfx {
class JavaBitmap;
}

namespace content {

// An interface to the browser-side compositor.
class CONTENT_EXPORT Compositor {
 public:
  class Client {
   public:
    // Tells the client that it should schedule a composite.
    virtual void ScheduleComposite() = 0;

    // The compositor has completed swapping a frame.
    virtual void OnSwapBuffersCompleted() {}

    // The compositor will eventually swap a frame.
    virtual void OnSwapBuffersPosted() {}
  };

  virtual ~Compositor() {}

  // Performs the global initialization needed before any compositor
  // instance can be used. This should be called only once.
  static void Initialize();

  enum CompositorFlags {
    // Creates a direct GL context on the thread that draws
    // (i.e. main or impl thread).
    DIRECT_CONTEXT_ON_DRAW_THREAD = 1,

    // Runs the compositor in threaded mode.
    ENABLE_COMPOSITOR_THREAD = 1 << 1,
  };

  // Initialize with flags. This should only be called once instead
  // of Initialize().
  static void InitializeWithFlags(uint32 flags);

  // Creates and returns a compositor instance.
  static Compositor* Create(Client* client);

  // Attaches the layer tree.
  virtual void SetRootLayer(scoped_refptr<cc::Layer> root) = 0;

  // Set the scale factor from DIP to pixel.
  virtual void setDeviceScaleFactor(float factor) = 0;

  // Set the output surface bounds.
  virtual void SetWindowBounds(const gfx::Size& size) = 0;

  // Sets the window visibility. When becoming invisible, resources will get
  // freed and other calls into the compositor are not allowed until after
  // having been made visible again.
  virtual void SetVisible(bool visible) = 0;

  // Set the output surface handle which the compositor renders into.
  // DEPRECATED: Use SetSurface() which takes a Java Surface object.
  virtual void SetWindowSurface(ANativeWindow* window) = 0;

  // Set the output surface which the compositor renders into.
  virtual void SetSurface(jobject surface) = 0;

  // Tells the view tree to assume a transparent background when rendering.
  virtual void SetHasTransparentBackground(bool flag) = 0;

  // Attempts to composite and read back the result into the provided buffer.
  // The buffer must be at least window width * height * 4 (RGBA) bytes large.
  // The buffer is not modified if false is returned.
  virtual bool CompositeAndReadback(void *pixels, const gfx::Rect& rect) = 0;

  // Invalidate the whole viewport.
  virtual void SetNeedsRedraw() = 0;

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

  // Grabs a copy of |texture_id| and saves it into |bitmap|.  No scaling is
  // done.  It is assumed that the texture size matches that of the bitmap.
  virtual bool CopyTextureToBitmap(WebKit::WebGLId texture_id,
                                   gfx::JavaBitmap& bitmap) = 0;

  // Grabs a copy of |texture_id| and saves it into |bitmap|.  No scaling is
  // done. |src_rect| allows the caller to specify which rect of |texture_id|
  // to copy to |bitmap|.  It needs to match the size of |bitmap|.  Returns
  // true if the |texture_id| was copied into |bitmap|, false if not.
  virtual bool CopyTextureToBitmap(WebKit::WebGLId texture_id,
                                   const gfx::Rect& src_rect,
                                   gfx::JavaBitmap& bitmap) = 0;
 protected:
  Compositor() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
