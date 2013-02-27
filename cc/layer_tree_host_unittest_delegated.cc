// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "cc/delegated_renderer_layer.h"
#include "cc/delegated_renderer_layer_impl.h"
#include "cc/layer_tree_impl.h"
#include "cc/test/layer_tree_test_common.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace cc {
namespace {

// These tests deal with delegated renderer layers.
class LayerTreeHostDelegatedTest : public ThreadedTest {};

class LayerTreeHostDelegatedTestCreateChildId
    : public LayerTreeHostDelegatedTest {
 public:
  virtual void setupTree() OVERRIDE {
    root_ = Layer::create();
    root_->setBounds(gfx::Size(10, 10));

    delegated_ = DelegatedRendererLayer::Create();
    delegated_->setBounds(gfx::Size(10, 10));
    delegated_->setIsDrawable(true);

    root_->addChild(delegated_);
    m_layerTreeHost->setRootLayer(root_);
    LayerTreeHostDelegatedTest::setupTree();
  }

  virtual void beginTest() OVERRIDE {
    num_activates_ = 0;
    did_reset_child_id_ = false;
    postSetNeedsCommitToMainThread();
  }

  virtual void treeActivatedOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerImpl* root_impl = host_impl->activeTree()->RootLayer();
    DelegatedRendererLayerImpl* delegated_impl =
        static_cast<DelegatedRendererLayerImpl*>(root_impl->children()[0]);

    ++num_activates_;
    switch(num_activates_) {
      case 1:
        EXPECT_TRUE(delegated_impl->child_id());
        EXPECT_FALSE(did_reset_child_id_);

        host_impl->resourceProvider()->graphicsContext3D()->loseContextCHROMIUM(
            GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
        break;
      case 2:
        EXPECT_TRUE(delegated_impl->child_id());
        EXPECT_TRUE(did_reset_child_id_);
        endTest();
        break;
    }
  }

  virtual void initializedRendererOnThread(LayerTreeHostImpl* host_impl,
                                           bool success) OVERRIDE {
    EXPECT_TRUE(success);

    if (!num_activates_)
      return;

    LayerImpl* root_impl = host_impl->activeTree()->RootLayer();
    DelegatedRendererLayerImpl* delegated_impl =
        static_cast<DelegatedRendererLayerImpl*>(root_impl->children()[0]);

    EXPECT_EQ(1, num_activates_);
    EXPECT_FALSE(delegated_impl->child_id());
    did_reset_child_id_ = true;
  }

  virtual void afterTest() OVERRIDE {}

 protected:
  int num_activates_;
  bool did_reset_child_id_;
  scoped_refptr<Layer> root_;
  scoped_refptr<DelegatedRendererLayer> delegated_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestCreateChildId)

}  // namespace
}  // namespace cc
