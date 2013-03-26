// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_CLIENT_H_
#define CC_TREES_LAYER_TREE_HOST_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace gfx {
class Vector2d;
}

namespace cc {
class ContextProvider;
class InputHandler;
class OutputSurface;

class LayerTreeHostClient {
 public:
  virtual void WillBeginFrame() = 0;
  // Marks finishing compositing-related tasks on the main thread. In threaded
  // mode, this corresponds to DidCommit().
  virtual void DidBeginFrame() = 0;
  virtual void Animate(double frame_begin_time) = 0;
  virtual void Layout() = 0;
  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta,
                                   float page_scale) = 0;
  virtual scoped_ptr<OutputSurface> CreateOutputSurface() = 0;
  virtual void DidRecreateOutputSurface(bool success) = 0;
  virtual scoped_ptr<InputHandler> CreateInputHandler() = 0;
  virtual void WillCommit() = 0;
  virtual void DidCommit() = 0;
  virtual void DidCommitAndDrawFrame() = 0;
  virtual void DidCompleteSwapBuffers() = 0;

  // Used only in the single-threaded path.
  virtual void ScheduleComposite() = 0;

  // These must always return a valid ContextProvider. But the provider does not
  // need to be capable of creating contexts.
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() = 0;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() = 0;

  // This hook is for testing.
  virtual void WillRetryRecreateOutputSurface() {}

 protected:
  virtual ~LayerTreeHostClient() {}
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_CLIENT_H_
