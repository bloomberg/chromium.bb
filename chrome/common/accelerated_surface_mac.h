// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACCELERATED_SURFACE_MAC_H_
#define CHROME_COMMON_ACCELERATED_SURFACE_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/OpenGL.h>

#include "base/callback.h"
#include "base/scoped_cftyperef.h"
#include "base/scoped_ptr.h"
#include "chrome/common/transport_dib.h"

namespace gfx {
class Rect;
}

// Encapsulates an accelerated GL surface that can be shared across processes
// on systems that support it (10.6 and above). For systems that do not, it
// uses a regular dib. There will either be a GL Context or a TransportDIB,
// never both.

class AcceleratedSurface {
 public:
  AcceleratedSurface();
  virtual ~AcceleratedSurface() { }

  // Set up internal buffers. Returns false upon failure.
  bool Initialize();
  // Tear down. Must be called before destructor to prevent leaks.
  void Destroy();

  // These methods are used only when there is a GL surface.

  // Sets the accelerated surface to the given size, creating a new one if
  // the height or width changes. Returns a unique id of the IOSurface to
  // which the surface is bound, or 0 if no changes were made or an error
  // occurred. MakeCurrent() will have been called on the new surface.
  uint64 SetSurfaceSize(int32 width, int32 height);
  // Sets the GL context to be the current one for drawing. Returns true if
  // it succeeded.
  bool MakeCurrent();
  // Clear the surface to all white. Assumes the caller has already called
  // MakeCurrent().
  void Clear(const gfx::Rect& rect);
  // Call after making changes to the surface which require a visual update.
  // Makes the rendering show up in other processes.
  void SwapBuffers();
  CGLContextObj context() { return gl_context_; }

  // These methods are only used when there is a transport DIB.

  // Sets the transport DIB to the given size, creating a new one if the
  // height or width changes. Returns a handle to the new DIB, or a default
  // handle if no changes were made.
  TransportDIB::Handle SetTransportDIBSize(int32 width, int32 height);
  // Sets the methods to use for allocating and freeing memory for the
  // transport DIB.
  void SetTransportDIBAllocAndFree(
      Callback2<size_t, TransportDIB::Handle*>::Type* allocator,
      Callback1<TransportDIB::Id>::Type* deallocator);

 private:
  // Helper function to generate names for the backing texture, render buffers
  // and FBO.  On return, the resulting buffer names can be attached to |fbo_|.
  // |target| is the target type for the color buffer.
  void AllocateRenderBuffers(GLenum target, int32 width, int32 height);

  // Helper function to attach the buffers previously allocated by a call to
  // AllocateRenderBuffers().  On return, |fbo_| can be used for
  // rendering.  |target| must be the same value as used in the call to
  // AllocateRenderBuffers().  Returns |true| if the resulting framebuffer
  // object is valid.
  bool SetupFrameBufferObject(GLenum target);

  CGLContextObj gl_context_;
  CGLPBufferObj pbuffer_;
  // Either |io_surface_| or |transport_dib_| is a valid pointer, but not both.
  // |io_surface_| is non-NULL if the IOSurface APIs are supported (Mac OS X
  // 10.6 and later).
  // TODO(dspringer,kbr): Should the GPU backing store be encapsulated in its
  // own class so all this implementation detail is hidden?
  scoped_cftyperef<CFTypeRef> io_surface_;
  // TODO(dspringer): If we end up keeping this TransportDIB mechanism, this
  // should really be a scoped_ptr_malloc<>, with a deallocate functor that
  // runs |dib_free_callback_|.  I was not able to figure out how to
  // make this work (or even compile).
  scoped_ptr<TransportDIB> transport_dib_;
  int32 surface_width_;
  int32 surface_height_;
  GLuint texture_;
  GLuint fbo_;
  GLuint depth_stencil_renderbuffer_;
  // For tracking whether the default framebuffer / renderbuffer or
  // ones created by the end user are currently bound
  // TODO(kbr): Need to property hook up and track the OpenGL state and hook
  // up the notion of the currently bound FBO.
  GLuint bound_fbo_;
  GLuint bound_renderbuffer_;
  // Allocate a TransportDIB in the renderer.
  scoped_ptr<Callback2<size_t, TransportDIB::Handle*>::Type>
      dib_alloc_callback_;
  scoped_ptr<Callback1<TransportDIB::Id>::Type> dib_free_callback_;
};

#endif  // CHROME_COMMON_ACCELERATED_SURFACE_MAC_H_
