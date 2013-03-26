// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "cc/input/input_handler.h"
#include "cc/test/fake_context_provider.h"
#include "cc/test/fake_output_surface.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

class FakeLayerTreeHostClient : public LayerTreeHostClient {
 public:
  enum RendererOptions {
    DIRECT_3D,
    DIRECT_SOFTWARE,
    DELEGATED_3D,
    DELEGATED_SOFTWARE
  };
  explicit FakeLayerTreeHostClient(RendererOptions options);
  virtual ~FakeLayerTreeHostClient();

  virtual void WillBeginFrame() OVERRIDE {}
  virtual void DidBeginFrame() OVERRIDE {}
  virtual void Animate(double frame_begin_time) OVERRIDE {}
  virtual void Layout() OVERRIDE {}
  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta,
                                   float page_scale) OVERRIDE {}

  virtual scoped_ptr<OutputSurface> CreateOutputSurface() OVERRIDE;
  virtual void DidRecreateOutputSurface(bool success) OVERRIDE {}
  virtual scoped_ptr<InputHandler> CreateInputHandler() OVERRIDE;
  virtual void WillCommit() OVERRIDE {}
  virtual void DidCommit() OVERRIDE {}
  virtual void DidCommitAndDrawFrame() OVERRIDE {}
  virtual void DidCompleteSwapBuffers() OVERRIDE {}

  // Used only in the single-threaded path.
  virtual void ScheduleComposite() OVERRIDE {}

  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE;

 private:
  bool use_software_rendering_;
  bool use_delegating_renderer_;

  scoped_refptr<FakeContextProvider> main_thread_contexts_;
  scoped_refptr<FakeContextProvider> compositor_thread_contexts_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_
