// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_PROCESS_H_
#define CONTENT_RENDERER_RENDER_PROCESS_H_

#include "content/common/child_process.h"
#include "skia/ext/platform_canvas.h"

class TransportDIB;

namespace gfx {
class Rect;
}

namespace content {

// A abstract interface representing the renderer end of the browser<->renderer
// connection. The opposite end is the RenderProcessHost. This is a singleton
// object for each renderer.
//
// RenderProcessImpl implements this interface for the regular browser.
// MockRenderProcess implements this interface for certain tests, especially
// ones derived from RenderViewTest.
class RenderProcess : public ChildProcess {
 public:
  RenderProcess() {}
  virtual ~RenderProcess() {}

  // Get a canvas suitable for drawing and transporting to the browser
  //   memory: (output) the transport DIB memory
  //   rect: the rectangle which will be painted, use for sizing the canvas
  //   returns: NULL on error
  //
  // When no longer needed, you should pass the TransportDIB to
  // ReleaseTransportDIB so that it can be recycled.
  virtual SkCanvas* GetDrawingCanvas(TransportDIB** memory,
                                     const gfx::Rect& rect) = 0;

  // Frees shared memory allocated by AllocSharedMemory.  You should only use
  // this function to free the SharedMemory object.
  virtual void ReleaseTransportDIB(TransportDIB* memory) = 0;

  // Returns true if plugisn should be loaded in-process.
  virtual bool UseInProcessPlugins() const = 0;

  // Keep track of the cumulative set of enabled bindings for this process,
  // across any view.
  virtual void AddBindings(int bindings) = 0;

  // The cumulative set of enabled bindings for this process.
  virtual int GetEnabledBindings() const = 0;

  // Create a new transport DIB of, at least, the given size. Return NULL on
  // error.
  virtual TransportDIB* CreateTransportDIB(size_t size) = 0;
  virtual void FreeTransportDIB(TransportDIB*) = 0;

  // Returns a pointer to the RenderProcess singleton instance. Assuming that
  // we're actually a renderer or a renderer test, this static cast will
  // be correct.
  static RenderProcess* current() {
    return static_cast<RenderProcess*>(ChildProcess::current());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderProcess);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_PROCESS_H_
