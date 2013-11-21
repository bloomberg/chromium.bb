// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_

#include "base/callback.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"

namespace cc {
class Layer;
}

namespace gfx {
class JavaBitmap;
}

namespace content {
class CompositorClient;

// An interface to the browser-side compositor.
class CONTENT_EXPORT Compositor {
 public:
  virtual ~Compositor() {}

  // Performs the global initialization needed before any compositor
  // instance can be used. This should be called only once.
  static void Initialize();

  // Creates and returns a compositor instance.  |root_window| needs to outlive
  // the compositor as it manages callbacks on the compositor.
  static Compositor* Create(CompositorClient* client,
                            gfx::NativeWindow root_window);

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

  // Attempts to composite and read back the result into the provided buffer.
  // The buffer must be at least window width * height * 4 (RGBA) bytes large.
  // The buffer is not modified if false is returned.
  virtual bool CompositeAndReadback(void *pixels, const gfx::Rect& rect) = 0;

  // Composite immediately. Used in single-threaded mode.
  virtual void Composite() = 0;

  // Generates a UIResource and returns a UIResourceId.  May return 0.
  virtual cc::UIResourceId GenerateUIResource(
      const cc::UIResourceBitmap& bitmap) = 0;

  // Deletes a UIResource.
  virtual void DeleteUIResource(cc::UIResourceId resource_id) = 0;

  // Generates an OpenGL texture and returns a texture handle.  May return 0
  // if the current context is lost.
  virtual blink::WebGLId GenerateTexture(gfx::JavaBitmap& bitmap) = 0;

  // Generates an OpenGL compressed texture and returns a texture handle.  May
  // return 0 if the current context is lost.
  virtual blink::WebGLId GenerateCompressedTexture(gfx::Size& size,
                                                    int data_size,
                                                    void* data) = 0;

  // Deletes an OpenGL texture.
  virtual void DeleteTexture(blink::WebGLId texture_id) = 0;

  // Grabs a copy of |texture_id| and saves it into |bitmap|.  No scaling is
  // done.  It is assumed that the texture size matches that of the bitmap.
  virtual bool CopyTextureToBitmap(blink::WebGLId texture_id,
                                   gfx::JavaBitmap& bitmap) = 0;

  // Grabs a copy of |texture_id| and saves it into |bitmap|.  No scaling is
  // done. |src_rect| allows the caller to specify which rect of |texture_id|
  // to copy to |bitmap|.  It needs to match the size of |bitmap|.  Returns
  // true if the |texture_id| was copied into |bitmap|, false if not.
  virtual bool CopyTextureToBitmap(blink::WebGLId texture_id,
                                   const gfx::Rect& src_rect,
                                   gfx::JavaBitmap& bitmap) = 0;
 protected:
  Compositor() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
