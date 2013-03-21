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
    MockLayerTreeHost(LayerTreeHostClient* client)
        : LayerTreeHost(client, LayerTreeSettings())
    {
        Initialize(scoped_ptr<Thread>(NULL));
    }

private:
};


class NinePatchLayerTest : public testing::Test {
public:
    NinePatchLayerTest()
        : m_fakeClient(FakeLayerTreeHostClient::DIRECT_3D)
    {
    }

    Proxy* proxy() const { return layer_tree_host_->proxy(); }

protected:
    virtual void SetUp()
    {
        layer_tree_host_.reset(new MockLayerTreeHost(&m_fakeClient));
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    }

    scoped_ptr<MockLayerTreeHost> layer_tree_host_;
    FakeLayerTreeHostClient m_fakeClient;
};

TEST_F(NinePatchLayerTest, triggerFullUploadOnceWhenChangingBitmap)
{
    scoped_refptr<NinePatchLayer> testLayer = NinePatchLayer::Create();
    ASSERT_TRUE(testLayer);
    testLayer->SetIsDrawable(true);
    testLayer->SetBounds(gfx::Size(100, 100));

    layer_tree_host_->SetRootLayer(testLayer);
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    EXPECT_EQ(testLayer->layer_tree_host(), layer_tree_host_.get());

    layer_tree_host_->InitializeRendererIfNeeded();

    PriorityCalculator calculator;
    ResourceUpdateQueue queue;
    OcclusionTracker occlusionTracker(gfx::Rect(), false);

    // No bitmap set should not trigger any uploads.
    testLayer->SetTexturePriorities(calculator);
    testLayer->Update(&queue, &occlusionTracker, NULL);
    EXPECT_EQ(queue.FullUploadSize(), 0);
    EXPECT_EQ(queue.PartialUploadSize(), 0);

    // Setting a bitmap set should trigger a single full upload.
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    bitmap.allocPixels();
    testLayer->SetBitmap(bitmap, gfx::Rect(5, 5, 1, 1));
    testLayer->SetTexturePriorities(calculator);
    testLayer->Update(&queue, &occlusionTracker, NULL);
    EXPECT_EQ(queue.FullUploadSize(), 1);
    EXPECT_EQ(queue.PartialUploadSize(), 0);
    ResourceUpdate params = queue.TakeFirstFullUpload();
    EXPECT_TRUE(params.texture != NULL);

    // Upload the texture.
    layer_tree_host_->contents_texture_manager()->SetMaxMemoryLimitBytes(1024 * 1024);
    layer_tree_host_->contents_texture_manager()->PrioritizeTextures();

    scoped_ptr<OutputSurface> outputSurface;
    scoped_ptr<ResourceProvider> resourceProvider;
    {
        DebugScopedSetImplThread implThread(proxy());
        DebugScopedSetMainThreadBlocked mainThreadBlocked(proxy());
        outputSurface = CreateFakeOutputSurface();
        resourceProvider = ResourceProvider::Create(outputSurface.get());
        params.texture->AcquireBackingTexture(resourceProvider.get());
        ASSERT_TRUE(params.texture->have_backing_texture());
    }

    // Nothing changed, so no repeated upload.
    testLayer->SetTexturePriorities(calculator);
    testLayer->Update(&queue, &occlusionTracker, NULL);
    EXPECT_EQ(queue.FullUploadSize(), 0);
    EXPECT_EQ(queue.PartialUploadSize(), 0);

    {
        DebugScopedSetImplThread implThread(proxy());
        DebugScopedSetMainThreadBlocked mainThreadBlocked(proxy());
        layer_tree_host_->contents_texture_manager()->ClearAllMemory(resourceProvider.get());
    }

    // Reupload after eviction
    testLayer->SetTexturePriorities(calculator);
    testLayer->Update(&queue, &occlusionTracker, NULL);
    EXPECT_EQ(queue.FullUploadSize(), 1);
    EXPECT_EQ(queue.PartialUploadSize(), 0);

    // PrioritizedResourceManager clearing
    layer_tree_host_->contents_texture_manager()->UnregisterTexture(params.texture);
    EXPECT_EQ(NULL, params.texture->resource_manager());
    testLayer->SetTexturePriorities(calculator);
    ResourceUpdateQueue queue2;
    testLayer->Update(&queue2, &occlusionTracker, NULL);
    EXPECT_EQ(queue2.FullUploadSize(), 1);
    EXPECT_EQ(queue2.PartialUploadSize(), 0);
    params = queue2.TakeFirstFullUpload();
    EXPECT_TRUE(params.texture != NULL);
    EXPECT_EQ(params.texture->resource_manager(), layer_tree_host_->contents_texture_manager());
}

}  // namespace
}  // namespace cc
