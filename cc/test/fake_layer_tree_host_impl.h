// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_H_

#include "cc/test/fake_layer_tree_host_impl_client.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/single_thread_proxy.h"

namespace cc {

class FakeLayerTreeHostImpl : public LayerTreeHostImpl {
 public:
  FakeLayerTreeHostImpl(Proxy* proxy, SharedBitmapManager* manager);
  FakeLayerTreeHostImpl(const LayerTreeSettings& settings,
                        Proxy* proxy,
                        SharedBitmapManager* manager);
  virtual ~FakeLayerTreeHostImpl();

  void ForcePrepareToDraw() {
    LayerTreeHostImpl::FrameData frame_data;
    PrepareToDraw(&frame_data);
    DidDrawAllLayers(frame_data);
  }

  virtual void CreatePendingTree() OVERRIDE;

  virtual BeginFrameArgs CurrentBeginFrameArgs() const OVERRIDE;
  void SetCurrentBeginFrameArgs(const BeginFrameArgs& args);
  void UpdateNumChildrenAndDrawPropertiesForActiveTree();
  static void UpdateNumChildrenAndDrawProperties(LayerTreeImpl* layerTree);
  static int RecursiveUpdateNumChildren(LayerImpl* layer);

  using LayerTreeHostImpl::ActivateSyncTree;
  using LayerTreeHostImpl::manage_tiles_needed;

 private:
  BeginFrameArgs current_begin_frame_args_;
  FakeLayerTreeHostImplClient client_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_H_
