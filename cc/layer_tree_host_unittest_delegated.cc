// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "cc/delegated_frame_data.h"
#include "cc/delegated_renderer_layer.h"
#include "cc/delegated_renderer_layer_impl.h"
#include "cc/layer_tree_impl.h"
#include "cc/test/layer_tree_test_common.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace cc {
namespace {

// These tests deal with delegated renderer layers.
class LayerTreeHostDelegatedTest : public ThreadedTest {
 protected:
  scoped_ptr<DelegatedFrameData> CreateFrameData(gfx::Rect root_output_rect,
                                                 gfx::Rect root_damage_rect) {
    scoped_ptr<DelegatedFrameData> frame(new DelegatedFrameData);

    scoped_ptr<RenderPass> root_pass(RenderPass::Create());
    root_pass->SetNew(RenderPass::Id(1, 1),
                      root_output_rect,
                      root_damage_rect,
                      gfx::Transform());
    frame->render_pass_list.push_back(root_pass.Pass());
    return frame.Pass();
  }

  scoped_ptr<DelegatedFrameData> CreateEmptyFrameData() {
    scoped_ptr<DelegatedFrameData> frame(new DelegatedFrameData);
    return frame.Pass();
  }
};

class LayerTreeHostDelegatedTestCaseSingleDelegatedLayer
    : public LayerTreeHostDelegatedTest {
 public:
  virtual void setupTree() OVERRIDE {
    root_ = Layer::create();
    root_->setAnchorPoint(gfx::PointF());
    root_->setBounds(gfx::Size(10, 10));

    delegated_ = DelegatedRendererLayer::Create();
    delegated_->setAnchorPoint(gfx::PointF());
    delegated_->setBounds(gfx::Size(10, 10));
    delegated_->setIsDrawable(true);

    root_->addChild(delegated_);
    m_layerTreeHost->setRootLayer(root_);
    LayerTreeHostDelegatedTest::setupTree();
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void afterTest() OVERRIDE {}

 protected:
  scoped_refptr<Layer> root_;
  scoped_refptr<DelegatedRendererLayer> delegated_;
};

class LayerTreeHostDelegatedTestCreateChildId
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  LayerTreeHostDelegatedTestCreateChildId()
      : LayerTreeHostDelegatedTestCaseSingleDelegatedLayer(),
        num_activates_(0),
        did_reset_child_id_(false) {}

  virtual void didCommit() OVERRIDE {
    if (testEnded())
      return;
    delegated_->SetFrameData(CreateFrameData(gfx::Rect(0, 0, 1, 1),
                                             gfx::Rect(0, 0, 1, 1)));
  }

  virtual void treeActivatedOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerImpl* root_impl = host_impl->activeTree()->RootLayer();
    DelegatedRendererLayerImpl* delegated_impl =
        static_cast<DelegatedRendererLayerImpl*>(root_impl->children()[0]);

    ++num_activates_;
    switch(num_activates_) {
      case 2:
        EXPECT_TRUE(delegated_impl->child_id());
        EXPECT_FALSE(did_reset_child_id_);

        host_impl->resourceProvider()->graphicsContext3D()->loseContextCHROMIUM(
            GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
        break;
      case 3:
        EXPECT_TRUE(delegated_impl->child_id());
        EXPECT_TRUE(did_reset_child_id_);
        endTest();
        break;
    }
  }

  virtual void initializedRendererOnThread(LayerTreeHostImpl* host_impl,
                                           bool success) OVERRIDE {
    EXPECT_TRUE(success);

    if (num_activates_ < 2)
      return;

    LayerImpl* root_impl = host_impl->activeTree()->RootLayer();
    DelegatedRendererLayerImpl* delegated_impl =
        static_cast<DelegatedRendererLayerImpl*>(root_impl->children()[0]);

    EXPECT_EQ(2, num_activates_);
    EXPECT_FALSE(delegated_impl->child_id());
    did_reset_child_id_ = true;
  }

  virtual void afterTest() OVERRIDE {}

 protected:
  int num_activates_;
  bool did_reset_child_id_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestCreateChildId)

class LayerTreeHostDelegatedTestLayerUsesFrameDamage
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  LayerTreeHostDelegatedTestLayerUsesFrameDamage()
      : LayerTreeHostDelegatedTestCaseSingleDelegatedLayer(),
        first_draw_for_source_frame_(true) {}

  virtual void didCommit() OVERRIDE {
    int next_source_frame_number = m_layerTreeHost->commitNumber();
    switch (next_source_frame_number) {
      case 1:
        // The first time the layer gets a frame the whole layer should be
        // damaged.
        delegated_->SetFrameData(CreateFrameData(gfx::Rect(0, 0, 1, 1),
                                                 gfx::Rect(0, 0, 1, 1)));
        break;
      case 2:
        // Should create a total amount of gfx::Rect(2, 2, 10, 6) damage.
        // The frame size is 20x20 while the layer is 10x10, so this should
        // produce a gfx::Rect(1, 1, 5, 3) damage rect.
        delegated_->SetFrameData(CreateFrameData(gfx::Rect(0, 0, 20, 20),
                                                 gfx::Rect(2, 2, 5, 5)));
        delegated_->SetFrameData(CreateFrameData(gfx::Rect(0, 0, 20, 20),
                                                 gfx::Rect(7, 2, 5, 6)));
        break;
      case 3:
        // Should create zero damage.
        m_layerTreeHost->setNeedsCommit();
        break;
      case 4:
        // Should damage the full viewport.
        delegated_->setBounds(gfx::Size(2, 2));
        break;
      case 5:
        // Should create zero damage.
        m_layerTreeHost->setNeedsCommit();
        break;
      case 6:
        // Should damage the full layer.
        delegated_->setBounds(gfx::Size(6, 6));
        delegated_->SetFrameData(CreateFrameData(gfx::Rect(0, 0, 5, 5),
                                                 gfx::Rect(1, 1, 2, 2)));
        break;
      case 7:
        // Should create zero damage.
        m_layerTreeHost->setNeedsCommit();
        break;
      case 8:
        // Should damage the full layer.
        delegated_->SetDisplaySize(gfx::Size(10, 10));
        break;
      case 9:
        // Should create zero damage.
        m_layerTreeHost->setNeedsCommit();
        break;
      case 10:
        // Setting an empty frame should damage the whole layer the
        // first time.
        delegated_->SetFrameData(CreateEmptyFrameData());
        break;
      case 11:
        // Setting an empty frame shouldn't damage anything after the
        // first time.
        delegated_->SetFrameData(CreateEmptyFrameData());
        break;
      case 12:
        // Having valid content to display agains should damage the whole layer.
        delegated_->SetFrameData(CreateFrameData(gfx::Rect(0, 0, 10, 10),
                                                 gfx::Rect(5, 5, 1, 1)));
        break;
      case 13:
        // Should create gfx::Rect(1, 1, 2, 2) of damage. The frame size is
        // 5x5 and the display size is now set to 10x10, so this should result
        // in a gfx::Rect(2, 2, 4, 4) damage rect.
        delegated_->SetFrameData(CreateFrameData(gfx::Rect(0, 0, 5, 5),
                                                 gfx::Rect(1, 1, 2, 2)));
        break;
      case 14:
        // Should create zero damage.
        m_layerTreeHost->setNeedsCommit();
        break;
    }
    first_draw_for_source_frame_ = true;
  }

  virtual bool prepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData& frame,
                                     bool result) {
    EXPECT_TRUE(result);

    if (!first_draw_for_source_frame_)
      return result;

    gfx::RectF damage_rect = frame.renderPasses.back()->damage_rect;

    switch (host_impl->activeTree()->source_frame_number()) {
      case 0:
        // Before the layer has a frame to display it should not
        // be visible at all, and not damage anything.
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 1:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 10.f, 10.f).ToString(),
                  damage_rect.ToString());
        break;
      case 2:
        EXPECT_EQ(gfx::RectF(1.f, 1.f, 5.f, 3.f).ToString(),
                  damage_rect.ToString());
        break;
      case 3:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 4:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 10.f, 10.f).ToString(),
                  damage_rect.ToString());
        break;
      case 5:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 6:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 6.f, 6.f).ToString(),
                  damage_rect.ToString());
        break;
      case 7:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 8:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 6.f, 6.f).ToString(),
                  damage_rect.ToString());
        break;
      case 9:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 10:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 6.f, 6.f).ToString(),
                  damage_rect.ToString());
        break;
      case 11:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 12:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 6.f, 6.f).ToString(),
                  damage_rect.ToString());
        break;
      case 13:
        EXPECT_EQ(gfx::RectF(2.f, 2.f, 4.f, 4.f).ToString(),
                  damage_rect.ToString());
        break;
      case 14:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        endTest();
        break;
    }

    return result;
  }

 protected:
  bool first_draw_for_source_frame_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestLayerUsesFrameDamage)

}  // namespace
}  // namespace cc
