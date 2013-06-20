// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer.h"

#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/scrollbar_layer_impl.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/priority_calculator.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_scrollbar.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

scoped_ptr<LayerImpl> LayerImplForScrollAreaAndScrollbar(
    FakeLayerTreeHostImpl* host_impl,
    scoped_ptr<Scrollbar> scrollbar,
    bool reverse_order) {
  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 =
      ScrollbarLayer::Create(scrollbar.Pass(),
                             child1->id());
  layer_tree_root->AddChild(child1);
  layer_tree_root->InsertChild(child2, reverse_order ? 0 : 1);
  scoped_ptr<LayerImpl> layer_impl =
      TreeSynchronizer::SynchronizeTrees(layer_tree_root.get(),
                                         scoped_ptr<LayerImpl>(),
                                         host_impl->active_tree());
  TreeSynchronizer::PushProperties(layer_tree_root.get(), layer_impl.get());
  return layer_impl.Pass();
}

TEST(ScrollbarLayerTest, ResolveScrollLayerPointer) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);

  {
    scoped_ptr<Scrollbar> scrollbar(new FakeScrollbar);
    scoped_ptr<LayerImpl> layer_impl_tree_root =
        LayerImplForScrollAreaAndScrollbar(
            &host_impl, scrollbar.Pass(), false);

    LayerImpl* cc_child1 = layer_impl_tree_root->children()[0];
    ScrollbarLayerImpl* cc_child2 = static_cast<ScrollbarLayerImpl*>(
        layer_impl_tree_root->children()[1]);

    EXPECT_EQ(cc_child1->horizontal_scrollbar_layer(), cc_child2);
  }
  {
    // another traverse order
    scoped_ptr<Scrollbar> scrollbar(new FakeScrollbar);
    scoped_ptr<LayerImpl> layer_impl_tree_root =
        LayerImplForScrollAreaAndScrollbar(
            &host_impl, scrollbar.Pass(), true);

    ScrollbarLayerImpl* cc_child1 = static_cast<ScrollbarLayerImpl*>(
        layer_impl_tree_root->children()[0]);
    LayerImpl* cc_child2 = layer_impl_tree_root->children()[1];

    EXPECT_EQ(cc_child2->horizontal_scrollbar_layer(), cc_child1);
  }
}

TEST(ScrollbarLayerTest, ShouldScrollNonOverlayOnMainThread) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);

  // Create and attach a non-overlay scrollbar.
  scoped_ptr<Scrollbar> scrollbar(new FakeScrollbar);
  scoped_ptr<LayerImpl> layer_impl_tree_root =
      LayerImplForScrollAreaAndScrollbar(&host_impl, scrollbar.Pass(), false);
  ScrollbarLayerImpl* scrollbar_layer_impl =
      static_cast<ScrollbarLayerImpl*>(layer_impl_tree_root->children()[1]);

  // When the scrollbar is not an overlay scrollbar, the scroll should be
  // responded to on the main thread as the compositor does not yet implement
  // scrollbar scrolling.
  EXPECT_EQ(InputHandler::ScrollOnMainThread,
            scrollbar_layer_impl->TryScroll(gfx::Point(0, 0),
                                            InputHandler::Gesture));

  // Create and attach an overlay scrollbar.
  scrollbar.reset(new FakeScrollbar(false, false, true));

  layer_impl_tree_root =
      LayerImplForScrollAreaAndScrollbar(&host_impl, scrollbar.Pass(), false);
  scrollbar_layer_impl =
      static_cast<ScrollbarLayerImpl*>(layer_impl_tree_root->children()[1]);

  // The user shouldn't be able to drag an overlay scrollbar and the scroll
  // may be handled in the compositor.
  EXPECT_EQ(InputHandler::ScrollIgnored,
            scrollbar_layer_impl->TryScroll(gfx::Point(0, 0),
                                            InputHandler::Gesture));
}

TEST(ScrollbarLayerTest, ScrollOffsetSynchronization) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);

  scoped_ptr<Scrollbar> scrollbar(new FakeScrollbar);
  scoped_refptr<Layer> layer_tree_root = Layer::Create();
  scoped_refptr<Layer> content_layer = Layer::Create();
  scoped_refptr<Layer> scrollbar_layer =
      ScrollbarLayer::Create(scrollbar.Pass(),
                             layer_tree_root->id());
  layer_tree_root->AddChild(content_layer);
  layer_tree_root->AddChild(scrollbar_layer);

  layer_tree_root->SetScrollOffset(gfx::Vector2d(10, 20));
  layer_tree_root->SetMaxScrollOffset(gfx::Vector2d(30, 50));
  layer_tree_root->SetBounds(gfx::Size(100, 200));
  layer_tree_root->SavePaintProperties();
  content_layer->SetBounds(gfx::Size(100, 200));
  content_layer->SavePaintProperties();

  scoped_ptr<LayerImpl> layer_impl_tree_root =
      TreeSynchronizer::SynchronizeTrees(layer_tree_root.get(),
                                         scoped_ptr<LayerImpl>(),
                                         host_impl.active_tree());
  TreeSynchronizer::PushProperties(layer_tree_root.get(),
                                   layer_impl_tree_root.get());

  ScrollbarLayerImpl* cc_scrollbar_layer =
      static_cast<ScrollbarLayerImpl*>(layer_impl_tree_root->children()[1]);

  EXPECT_EQ(10.f, cc_scrollbar_layer->CurrentPos());
  EXPECT_EQ(30, cc_scrollbar_layer->Maximum());

  layer_tree_root->SetScrollOffset(gfx::Vector2d(100, 200));
  layer_tree_root->SetMaxScrollOffset(gfx::Vector2d(300, 500));
  layer_tree_root->SetBounds(gfx::Size(1000, 2000));
  layer_tree_root->SavePaintProperties();
  content_layer->SetBounds(gfx::Size(1000, 2000));
  content_layer->SavePaintProperties();

  ScrollbarAnimationController* scrollbar_controller =
      layer_impl_tree_root->scrollbar_animation_controller();
  layer_impl_tree_root =
      TreeSynchronizer::SynchronizeTrees(layer_tree_root.get(),
                                         layer_impl_tree_root.Pass(),
                                         host_impl.active_tree());
  TreeSynchronizer::PushProperties(layer_tree_root.get(),
                                   layer_impl_tree_root.get());
  EXPECT_EQ(scrollbar_controller,
            layer_impl_tree_root->scrollbar_animation_controller());

  EXPECT_EQ(100.f, cc_scrollbar_layer->CurrentPos());
  EXPECT_EQ(300, cc_scrollbar_layer->Maximum());

  layer_impl_tree_root->ScrollBy(gfx::Vector2d(12, 34));

  EXPECT_EQ(112.f, cc_scrollbar_layer->CurrentPos());
  EXPECT_EQ(300, cc_scrollbar_layer->Maximum());
}

TEST(ScrollbarLayerTest, SolidColorDrawQuads) {
  LayerTreeSettings layer_tree_settings;
  layer_tree_settings.solid_color_scrollbars = true;
  layer_tree_settings.solid_color_scrollbar_thickness_dip = 3;
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(layer_tree_settings, &proxy);

  scoped_ptr<Scrollbar> scrollbar(new FakeScrollbar(false, true, true));
  scoped_ptr<LayerImpl> layer_impl_tree_root =
      LayerImplForScrollAreaAndScrollbar(&host_impl, scrollbar.Pass(), false);
  ScrollbarLayerImpl* scrollbar_layer_impl =
      static_cast<ScrollbarLayerImpl*>(layer_impl_tree_root->children()[1]);
  scrollbar_layer_impl->set_thumb_thickness(3);
  scrollbar_layer_impl->SetCurrentPos(10.f);
  scrollbar_layer_impl->SetMaximum(100);
  scrollbar_layer_impl->set_track_length(100);
  scrollbar_layer_impl->set_visible_to_total_length_ratio(0.4f);

  // Thickness should be overridden to 3.
  {
    MockQuadCuller quad_culler;
    AppendQuadsData data;
    scrollbar_layer_impl->AppendQuads(&quad_culler, &data);

    const QuadList& quads = quad_culler.quad_list();
    ASSERT_EQ(1u, quads.size());
    EXPECT_EQ(DrawQuad::SOLID_COLOR, quads[0]->material);
    EXPECT_RECT_EQ(gfx::Rect(6, 0, 40, 3), quads[0]->rect);
  }

  // Contents scale should scale the draw quad.
  scrollbar_layer_impl->draw_properties().contents_scale_x = 2.f;
  scrollbar_layer_impl->draw_properties().contents_scale_y = 2.f;
  {
    MockQuadCuller quad_culler;
    AppendQuadsData data;
    scrollbar_layer_impl->AppendQuads(&quad_culler, &data);

    const QuadList& quads = quad_culler.quad_list();
    ASSERT_EQ(1u, quads.size());
    EXPECT_EQ(DrawQuad::SOLID_COLOR, quads[0]->material);
    EXPECT_RECT_EQ(gfx::Rect(12, 0, 80, 6), quads[0]->rect);
  }
  scrollbar_layer_impl->draw_properties().contents_scale_x = 1.f;
  scrollbar_layer_impl->draw_properties().contents_scale_y = 1.f;

  // For solid color scrollbars, position and size should reflect the
  // current viewport state.
  scrollbar_layer_impl->set_visible_to_total_length_ratio(0.2f);
  {
    MockQuadCuller quad_culler;
    AppendQuadsData data;
    scrollbar_layer_impl->AppendQuads(&quad_culler, &data);

    const QuadList& quads = quad_culler.quad_list();
    ASSERT_EQ(1u, quads.size());
    EXPECT_EQ(DrawQuad::SOLID_COLOR, quads[0]->material);
    EXPECT_RECT_EQ(gfx::Rect(8, 0, 20, 3), quads[0]->rect);
  }
}

TEST(ScrollbarLayerTest, LayerDrivenSolidColorDrawQuads) {
  LayerTreeSettings layer_tree_settings;
  layer_tree_settings.solid_color_scrollbars = true;
  layer_tree_settings.solid_color_scrollbar_thickness_dip = 3;
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(layer_tree_settings, &proxy);

  scoped_ptr<Scrollbar> scrollbar(new FakeScrollbar(false, true, true));
  scoped_ptr<LayerImpl> layer_impl_tree_root =
      LayerImplForScrollAreaAndScrollbar(&host_impl, scrollbar.Pass(), false);
  ScrollbarLayerImpl* scrollbar_layer_impl =
      static_cast<ScrollbarLayerImpl*>(layer_impl_tree_root->children()[1]);

  scrollbar_layer_impl->set_thumb_thickness(3);
  scrollbar_layer_impl->set_track_length(10);
  scrollbar_layer_impl->SetCurrentPos(4.f);
  scrollbar_layer_impl->SetMaximum(8);

  layer_impl_tree_root->SetHorizontalScrollbarLayer(scrollbar_layer_impl);
  layer_impl_tree_root->SetMaxScrollOffset(gfx::Vector2d(8, 8));
  layer_impl_tree_root->SetBounds(gfx::Size(2, 2));
  layer_impl_tree_root->ScrollBy(gfx::Vector2dF(4.f, 0.f));

  {
    MockQuadCuller quad_culler;
    AppendQuadsData data;
    scrollbar_layer_impl->AppendQuads(&quad_culler, &data);

    const QuadList& quads = quad_culler.quad_list();
    ASSERT_EQ(1u, quads.size());
    EXPECT_EQ(DrawQuad::SOLID_COLOR, quads[0]->material);
    EXPECT_RECT_EQ(gfx::Rect(3, 0, 3, 3), quads[0]->rect);
  }
}

class ScrollbarLayerSolidColorThumbTest : public testing::Test {
 public:
  ScrollbarLayerSolidColorThumbTest() {
    LayerTreeSettings layer_tree_settings;
    layer_tree_settings.solid_color_scrollbars = true;
    host_impl_.reset(new FakeLayerTreeHostImpl(layer_tree_settings, &proxy_));

    horizontal_scrollbar_layer_ = ScrollbarLayerImpl::Create(
        host_impl_->active_tree(), 1, HORIZONTAL);
    vertical_scrollbar_layer_ = ScrollbarLayerImpl::Create(
        host_impl_->active_tree(), 2, VERTICAL);
  }

 protected:
  FakeImplProxy proxy_;
  scoped_ptr<FakeLayerTreeHostImpl> host_impl_;
  scoped_ptr<ScrollbarLayerImpl> horizontal_scrollbar_layer_;
  scoped_ptr<ScrollbarLayerImpl> vertical_scrollbar_layer_;
};

TEST_F(ScrollbarLayerSolidColorThumbTest, SolidColorThumbLength) {
  horizontal_scrollbar_layer_->SetCurrentPos(0);
  horizontal_scrollbar_layer_->SetMaximum(10);
  horizontal_scrollbar_layer_->set_thumb_thickness(3);

  // Simple case - one third of the scrollable area is visible, so the thumb
  // should be one third as long as the track.
  horizontal_scrollbar_layer_->set_visible_to_total_length_ratio(0.33f);
  horizontal_scrollbar_layer_->set_track_length(100);
  EXPECT_EQ(33, horizontal_scrollbar_layer_->ComputeThumbQuadRect().width());

  // The thumb's length should never be less than its thickness.
  horizontal_scrollbar_layer_->set_visible_to_total_length_ratio(0.01f);
  horizontal_scrollbar_layer_->set_track_length(100);
  EXPECT_EQ(3, horizontal_scrollbar_layer_->ComputeThumbQuadRect().width());
}

TEST_F(ScrollbarLayerSolidColorThumbTest, SolidColorThumbPosition) {
  horizontal_scrollbar_layer_->set_track_length(100);
  horizontal_scrollbar_layer_->set_visible_to_total_length_ratio(0.1f);
  horizontal_scrollbar_layer_->set_thumb_thickness(3);

  horizontal_scrollbar_layer_->SetCurrentPos(0);
  horizontal_scrollbar_layer_->SetMaximum(100);
  EXPECT_EQ(0, horizontal_scrollbar_layer_->ComputeThumbQuadRect().x());
  EXPECT_EQ(10, horizontal_scrollbar_layer_->ComputeThumbQuadRect().width());

  horizontal_scrollbar_layer_->SetCurrentPos(100);
  // The thumb is 10px long and the track is 100px, so the maximum thumb
  // position is 90px.
  EXPECT_EQ(90, horizontal_scrollbar_layer_->ComputeThumbQuadRect().x());

  horizontal_scrollbar_layer_->SetCurrentPos(80);
  // The scroll position is 80% of the maximum, so the thumb's position should
  // be at 80% of its maximum or 72px.
  EXPECT_EQ(72, horizontal_scrollbar_layer_->ComputeThumbQuadRect().x());
}

TEST_F(ScrollbarLayerSolidColorThumbTest, SolidColorThumbVerticalAdjust) {
  ScrollbarLayerImpl* layers[2] =
      { horizontal_scrollbar_layer_.get(), vertical_scrollbar_layer_.get() };
  for (size_t i = 0; i < 2; ++i) {
    layers[i]->set_track_length(100);
    layers[i]->set_visible_to_total_length_ratio(0.2f);
    layers[i]->set_thumb_thickness(3);
    layers[i]->SetCurrentPos(25);
    layers[i]->SetMaximum(100);
  }

  EXPECT_RECT_EQ(gfx::RectF(20.f, 0.f, 20.f, 3.f),
                 horizontal_scrollbar_layer_->ComputeThumbQuadRect());
  EXPECT_RECT_EQ(gfx::RectF(0.f, 20.f, 3.f, 20.f),
                 vertical_scrollbar_layer_->ComputeThumbQuadRect());

  horizontal_scrollbar_layer_->set_vertical_adjust(10.f);
  vertical_scrollbar_layer_->set_vertical_adjust(10.f);

  // The vertical adjustment factor has two effects:
  // 1.) Moves the horizontal scrollbar down
  // 2.) Increases the vertical scrollbar's effective track length which both
  // increases the thumb's length and its position within the track.
  EXPECT_RECT_EQ(gfx::Rect(20.f, 10.f, 20.f, 3.f),
                 horizontal_scrollbar_layer_->ComputeThumbQuadRect());
  EXPECT_RECT_EQ(gfx::Rect(0.f, 22, 3.f, 22.f),
                 vertical_scrollbar_layer_->ComputeThumbQuadRect());
}

class ScrollbarLayerTestMaxTextureSize : public LayerTreeTest {
 public:
  ScrollbarLayerTestMaxTextureSize() {}

  void SetScrollbarBounds(gfx::Size bounds) { bounds_ = bounds; }

  virtual void BeginTest() OVERRIDE {
    scoped_ptr<Scrollbar> scrollbar(new FakeScrollbar);
    scrollbar_layer_ = ScrollbarLayer::Create(scrollbar.Pass(), 1);
    scrollbar_layer_->SetLayerTreeHost(layer_tree_host());
    scrollbar_layer_->SetBounds(bounds_);
    layer_tree_host()->root_layer()->AddChild(scrollbar_layer_);

    scroll_layer_ = Layer::Create();
    scrollbar_layer_->SetScrollLayerId(scroll_layer_->id());
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    const int kMaxTextureSize =
        layer_tree_host()->GetRendererCapabilities().max_texture_size;

    // Check first that we're actually testing something.
    EXPECT_GT(scrollbar_layer_->bounds().width(), kMaxTextureSize);

    EXPECT_EQ(scrollbar_layer_->content_bounds().width(),
              kMaxTextureSize - 1);
    EXPECT_EQ(scrollbar_layer_->content_bounds().height(),
              kMaxTextureSize - 1);

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  scoped_refptr<ScrollbarLayer> scrollbar_layer_;
  scoped_refptr<Layer> scroll_layer_;
  gfx::Size bounds_;
};

TEST_F(ScrollbarLayerTestMaxTextureSize, DirectRenderer) {
  scoped_ptr<TestWebGraphicsContext3D> context =
      TestWebGraphicsContext3D::Create();
  int max_size = 0;
  context->getIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
  SetScrollbarBounds(gfx::Size(max_size + 100, max_size + 100));
  RunTest(true, false, true);
}

TEST_F(ScrollbarLayerTestMaxTextureSize, DelegatingRenderer) {
  scoped_ptr<TestWebGraphicsContext3D> context =
      TestWebGraphicsContext3D::Create();
  int max_size = 0;
  context->getIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
  SetScrollbarBounds(gfx::Size(max_size + 100, max_size + 100));
  RunTest(true, true, true);
}

class MockLayerTreeHost : public LayerTreeHost {
 public:
  MockLayerTreeHost(LayerTreeHostClient* client,
                    const LayerTreeSettings& settings)
      : LayerTreeHost(client, settings) {
    Initialize(NULL);
  }
};


class ScrollbarLayerTestResourceCreation : public testing::Test {
 public:
  ScrollbarLayerTestResourceCreation()
      : fake_client_(FakeLayerTreeHostClient::DIRECT_3D) {}

  void TestResourceUpload(size_t expected_resources) {
    layer_tree_host_.reset(
        new MockLayerTreeHost(&fake_client_, layer_tree_settings_));

    scoped_ptr<Scrollbar> scrollbar(new FakeScrollbar(false, true, false));
    scoped_refptr<Layer> layer_tree_root = Layer::Create();
    scoped_refptr<Layer> content_layer = Layer::Create();
    scoped_refptr<Layer> scrollbar_layer =
        ScrollbarLayer::Create(scrollbar.Pass(), layer_tree_root->id());
    layer_tree_root->AddChild(content_layer);
    layer_tree_root->AddChild(scrollbar_layer);

    layer_tree_host_->InitializeOutputSurfaceIfNeeded();
    layer_tree_host_->contents_texture_manager()->
        SetMaxMemoryLimitBytes(1024 * 1024);
    layer_tree_host_->SetRootLayer(layer_tree_root);

    scrollbar_layer->SetIsDrawable(true);
    scrollbar_layer->SetBounds(gfx::Size(100, 100));
    layer_tree_root->SetScrollOffset(gfx::Vector2d(10, 20));
    layer_tree_root->SetMaxScrollOffset(gfx::Vector2d(30, 50));
    layer_tree_root->SetBounds(gfx::Size(100, 200));
    content_layer->SetBounds(gfx::Size(100, 200));
    scrollbar_layer->draw_properties().content_bounds = gfx::Size(100, 200);
    scrollbar_layer->draw_properties().visible_content_rect =
        gfx::Rect(0, 0, 100, 200);
    scrollbar_layer->CreateRenderSurface();
    scrollbar_layer->draw_properties().render_target = scrollbar_layer.get();

    testing::Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    EXPECT_EQ(scrollbar_layer->layer_tree_host(), layer_tree_host_.get());

    PriorityCalculator calculator;
    ResourceUpdateQueue queue;
    OcclusionTracker occlusion_tracker(gfx::Rect(), false);

    scrollbar_layer->SetTexturePriorities(calculator);
    layer_tree_host_->contents_texture_manager()->PrioritizeTextures();
    scrollbar_layer->Update(&queue, &occlusion_tracker, NULL);
    EXPECT_EQ(0u, queue.FullUploadSize());
    EXPECT_EQ(expected_resources, queue.PartialUploadSize());

    testing::Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  }

 protected:
  FakeLayerTreeHostClient fake_client_;
  LayerTreeSettings layer_tree_settings_;
  scoped_ptr<MockLayerTreeHost> layer_tree_host_;
};

TEST_F(ScrollbarLayerTestResourceCreation, ResourceUpload) {
  layer_tree_settings_.solid_color_scrollbars = false;
  TestResourceUpload(2);
}

TEST_F(ScrollbarLayerTestResourceCreation, SolidColorNoResourceUpload) {
  layer_tree_settings_.solid_color_scrollbars = true;
  TestResourceUpload(0);
}

class ScaledScrollbarLayerTestResourceCreation : public testing::Test {
 public:
  ScaledScrollbarLayerTestResourceCreation()
      : fake_client_(FakeLayerTreeHostClient::DIRECT_3D) {}

  void TestResourceUpload(size_t expected_resources, const float test_scale) {
    layer_tree_host_.reset(
        new MockLayerTreeHost(&fake_client_, layer_tree_settings_));

    gfx::Point scrollbar_location(0, 185);
    scoped_ptr<FakeScrollbar> scrollbar(new FakeScrollbar(false, true, false));
    scrollbar->set_location(scrollbar_location);

    scoped_refptr<Layer> layer_tree_root = Layer::Create();
    scoped_refptr<Layer> content_layer = Layer::Create();
    scoped_refptr<Layer> scrollbar_layer =
        ScrollbarLayer::Create(scrollbar.PassAs<cc::Scrollbar>(),
                               layer_tree_root->id());
    layer_tree_root->AddChild(content_layer);
    layer_tree_root->AddChild(scrollbar_layer);

    layer_tree_host_->InitializeOutputSurfaceIfNeeded();
    layer_tree_host_->contents_texture_manager()->
        SetMaxMemoryLimitBytes(1024 * 1024);
    layer_tree_host_->SetRootLayer(layer_tree_root);

    scrollbar_layer->SetIsDrawable(true);
    scrollbar_layer->SetBounds(gfx::Size(100, 15));
    scrollbar_layer->SetPosition(scrollbar_location);
    layer_tree_root->SetBounds(gfx::Size(100, 200));
    content_layer->SetBounds(gfx::Size(100, 200));
    gfx::SizeF scaled_size =
        gfx::ScaleSize(scrollbar_layer->bounds(), test_scale, test_scale);
    gfx::PointF scaled_location =
        gfx::ScalePoint(scrollbar_layer->position(), test_scale, test_scale);
    scrollbar_layer->draw_properties().content_bounds =
        gfx::Size(scaled_size.width(), scaled_size.height());
    scrollbar_layer->draw_properties().contents_scale_x = test_scale;
    scrollbar_layer->draw_properties().contents_scale_y = test_scale;
    scrollbar_layer->draw_properties().visible_content_rect =
        gfx::Rect(scaled_location.x(),
                  scaled_location.y(),
                  scaled_size.width(),
                  scaled_size.height());
    scrollbar_layer->CreateRenderSurface();
    scrollbar_layer->draw_properties().render_target = scrollbar_layer.get();

    testing::Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    EXPECT_EQ(scrollbar_layer->layer_tree_host(), layer_tree_host_.get());

    PriorityCalculator calculator;
    ResourceUpdateQueue queue;
    OcclusionTracker occlusion_tracker(gfx::Rect(), false);

    scrollbar_layer->SetTexturePriorities(calculator);
    layer_tree_host_->contents_texture_manager()->PrioritizeTextures();
    scrollbar_layer->Update(&queue, &occlusion_tracker, NULL);
    EXPECT_EQ(expected_resources, queue.PartialUploadSize());

    // Verify that we have not generated any content uploads that are larger
    // than their destination textures.
    while (queue.HasMoreUpdates()) {
      ResourceUpdate update = queue.TakeFirstPartialUpload();
      EXPECT_LE(update.texture->size().width(),
                scrollbar_layer->content_bounds().width());
      EXPECT_LE(update.texture->size().height(),
                scrollbar_layer->content_bounds().height());

      EXPECT_LE(update.dest_offset.x() + update.content_rect.width(),
                update.texture->size().width());
      EXPECT_LE(update.dest_offset.y() + update.content_rect.height(),
                update.texture->size().height());
    }

    testing::Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  }

 protected:
  FakeLayerTreeHostClient fake_client_;
  LayerTreeSettings layer_tree_settings_;
  scoped_ptr<MockLayerTreeHost> layer_tree_host_;
};

TEST_F(ScaledScrollbarLayerTestResourceCreation, ScaledResourceUpload) {
  layer_tree_settings_.solid_color_scrollbars = false;
  // Pick a test scale that moves the scrollbar's (non-zero) position to
  // a non-pixel-aligned location.
  TestResourceUpload(2, 1.41f);
}

}  // namespace
}  // namespace cc
