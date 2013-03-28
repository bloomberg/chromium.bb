// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_layer.h"

#include "cc/base/thread.h"
#include "cc/debug/overdraw_metrics.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/scheduler/texture_uploader.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace cc {
namespace {

class MockLayerTreeHost : public LayerTreeHost {
 public:
  explicit MockLayerTreeHost(LayerTreeHostClient* client)
      : LayerTreeHost(client, LayerTreeSettings()) {
    Initialize(scoped_ptr<Thread>(NULL));
  }
};

class NinePatchLayerTest : public testing::Test {
 public:
  NinePatchLayerTest() : fake_client_(FakeLayerTreeHostClient::DIRECT_3D) {}

  cc::Proxy* Proxy() const { return layer_tree_host_->proxy(); }

 protected:
  virtual void SetUp() {
    layer_tree_host_.reset(new MockLayerTreeHost(&fake_client_));
  }

  virtual void TearDown() {
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  }

  scoped_ptr<MockLayerTreeHost> layer_tree_host_;
  FakeLayerTreeHostClient fake_client_;
};

TEST_F(NinePatchLayerTest, TriggerFullUploadOnceWhenChangingBitmap) {
  scoped_refptr<NinePatchLayer> test_layer = NinePatchLayer::Create();
  ASSERT_TRUE(test_layer);
  test_layer->SetIsDrawable(true);
  test_layer->SetBounds(gfx::Size(100, 100));

  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host_.get());

  layer_tree_host_->InitializeRendererIfNeeded();

  PriorityCalculator calculator;
  ResourceUpdateQueue queue;
  OcclusionTracker occlusion_tracker(gfx::Rect(), false);

  // No bitmap set should not trigger any uploads.
  test_layer->SetTexturePriorities(calculator);
  test_layer->Update(&queue, &occlusion_tracker);
  EXPECT_EQ(queue.FullUploadSize(), 0);
  EXPECT_EQ(queue.PartialUploadSize(), 0);

  // Setting a bitmap set should trigger a single full upload.
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
  bitmap.allocPixels();
  test_layer->SetBitmap(bitmap, gfx::Rect(5, 5, 1, 1));
  test_layer->SetTexturePriorities(calculator);
  test_layer->Update(&queue, &occlusion_tracker);
  EXPECT_EQ(queue.FullUploadSize(), 1);
  EXPECT_EQ(queue.PartialUploadSize(), 0);
  ResourceUpdate params = queue.TakeFirstFullUpload();
  EXPECT_TRUE(params.texture != NULL);

  // Upload the texture.
  layer_tree_host_->contents_texture_manager()->SetMaxMemoryLimitBytes(
      1024 * 1024);
  layer_tree_host_->contents_texture_manager()->PrioritizeTextures();

  scoped_ptr<OutputSurface> output_surface;
  scoped_ptr<ResourceProvider> resource_provider;
  {
    DebugScopedSetImplThread impl_thread(Proxy());
    DebugScopedSetMainThreadBlocked main_thread_blocked(Proxy());
    output_surface = CreateFakeOutputSurface();
    resource_provider = ResourceProvider::Create(output_surface.get());
    params.texture->AcquireBackingTexture(resource_provider.get());
    ASSERT_TRUE(params.texture->have_backing_texture());
  }

  // Nothing changed, so no repeated upload.
  test_layer->SetTexturePriorities(calculator);
  test_layer->Update(&queue, &occlusion_tracker);
  EXPECT_EQ(queue.FullUploadSize(), 0);
  EXPECT_EQ(queue.PartialUploadSize(), 0);
  {
    DebugScopedSetImplThread impl_thread(Proxy());
    DebugScopedSetMainThreadBlocked main_thread_blocked(Proxy());
    layer_tree_host_->contents_texture_manager()->ClearAllMemory(
        resource_provider.get());
  }

  // Reupload after eviction
  test_layer->SetTexturePriorities(calculator);
  test_layer->Update(&queue, &occlusion_tracker);
  EXPECT_EQ(queue.FullUploadSize(), 1);
  EXPECT_EQ(queue.PartialUploadSize(), 0);

  // PrioritizedResourceManager clearing
  layer_tree_host_->contents_texture_manager()->UnregisterTexture(
      params.texture);
  EXPECT_EQ(NULL, params.texture->resource_manager());
  test_layer->SetTexturePriorities(calculator);
  ResourceUpdateQueue queue2;
  test_layer->Update(&queue2, &occlusion_tracker);
  EXPECT_EQ(queue2.FullUploadSize(), 1);
  EXPECT_EQ(queue2.PartialUploadSize(), 0);
  params = queue2.TakeFirstFullUpload();
  EXPECT_TRUE(params.texture != NULL);
  EXPECT_EQ(params.texture->resource_manager(),
            layer_tree_host_->contents_texture_manager());
}

}  // namespace
}  // namespace cc
