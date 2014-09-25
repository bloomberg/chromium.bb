// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "cc/input/input_handler.h"
#include "cc/test/test_context_provider.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"

namespace cc {
class OutputSurface;

class FakeLayerTreeHostClient : public LayerTreeHostClient,
                                public LayerTreeHostSingleThreadClient {
 public:
  enum RendererOptions {
    DIRECT_3D,
    DIRECT_SOFTWARE,
    DELEGATED_3D,
    DELEGATED_SOFTWARE
  };
  explicit FakeLayerTreeHostClient(RendererOptions options);
  virtual ~FakeLayerTreeHostClient();

  // Caller responsible for unsetting this and maintaining the host's lifetime.
  void SetLayerTreeHost(LayerTreeHost* host) { host_ = host; }

  // LayerTreeHostClient implementation.
  virtual void WillBeginMainFrame(int frame_id) OVERRIDE {}
  virtual void DidBeginMainFrame() OVERRIDE {}
  virtual void BeginMainFrame(const BeginFrameArgs& args) OVERRIDE {}
  virtual void Layout() OVERRIDE {}
  virtual void ApplyViewportDeltas(const gfx::Vector2d& scroll_delta,
                                   float page_scale,
                                   float top_controls_delta) OVERRIDE {}

  virtual void RequestNewOutputSurface(bool fallback) OVERRIDE;
  virtual void DidInitializeOutputSurface() OVERRIDE {}
  virtual void WillCommit() OVERRIDE {}
  virtual void DidCommit() OVERRIDE {}
  virtual void DidCommitAndDrawFrame() OVERRIDE {}
  virtual void DidCompleteSwapBuffers() OVERRIDE {}

  // LayerTreeHostSingleThreadClient implementation.
  virtual void DidPostSwapBuffers() OVERRIDE {}
  virtual void DidAbortSwapBuffers() OVERRIDE {}

 private:
  bool use_software_rendering_;
  bool use_delegating_renderer_;

  LayerTreeHost* host_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_
