// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_factory.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/resource_provider.h"
#include "cc/surfaces/frame_sink_manager_client.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_info.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_resource_holder_client.h"
#include "cc/test/fake_surface_resource_holder_client.h"
#include "cc/test/scheduler_test_common.h"
#include "cc/test/stub_surface_factory_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);
static constexpr FrameSinkId kAnotherArbitraryFrameSinkId(2, 2);
static const base::UnguessableToken kArbitraryToken =
    base::UnguessableToken::Create();
static auto kArbitrarySourceId1 =
    base::UnguessableToken::Deserialize(0xdead, 0xbeef);
static auto kArbitrarySourceId2 =
    base::UnguessableToken::Deserialize(0xdead, 0xbee0);

gpu::SyncToken GenTestSyncToken(int id) {
  gpu::SyncToken token;
  token.Set(gpu::CommandBufferNamespace::GPU_IO, 0,
            gpu::CommandBufferId::FromUnsafeValue(id), 1);
  return token;
}

class SurfaceFactoryTest : public testing::Test, public SurfaceObserver {
 public:
  SurfaceFactoryTest()
      : factory_(new SurfaceFactory(kArbitraryFrameSinkId,
                                    &manager_,
                                    &stub_surface_factory_client_,
                                    &fake_surface_resource_holder_client_)),
        local_surface_id_(3, kArbitraryToken),
        frame_sync_token_(GenTestSyncToken(4)),
        consumer_sync_token_(GenTestSyncToken(5)) {
    manager_.AddObserver(this);
  }

  const SurfaceId& last_created_surface_id() const {
    return last_created_surface_id_;
  }

  // SurfaceObserver implementation.
  void OnSurfaceCreated(const SurfaceInfo& surface_info) override {
    EXPECT_EQ(kArbitraryFrameSinkId, surface_info.id().frame_sink_id());
    last_created_surface_id_ = surface_info.id();
    last_surface_info_ = surface_info;
  }

  void OnSurfaceDamaged(const SurfaceId& id, bool* changed) override {
    *changed = true;
  }

  ~SurfaceFactoryTest() override {
    manager_.RemoveObserver(this);
    factory_->EvictSurface();
  }

  void SubmitCompositorFrameWithResources(ResourceId* resource_ids,
                                          size_t num_resource_ids) {
    CompositorFrame frame;
    for (size_t i = 0u; i < num_resource_ids; ++i) {
      TransferableResource resource;
      resource.id = resource_ids[i];
      resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
      resource.mailbox_holder.sync_token = frame_sync_token_;
      frame.resource_list.push_back(resource);
    }
    factory_->SubmitCompositorFrame(local_surface_id_, std::move(frame),
                                    SurfaceFactory::DrawCallback(),
                                    SurfaceFactory::WillDrawCallback());
    EXPECT_EQ(last_created_surface_id_.local_surface_id(), local_surface_id_);
  }

  void UnrefResources(ResourceId* ids_to_unref,
                      int* counts_to_unref,
                      size_t num_ids_to_unref) {
    ReturnedResourceArray unref_array;
    for (size_t i = 0; i < num_ids_to_unref; ++i) {
      ReturnedResource resource;
      resource.sync_token = consumer_sync_token_;
      resource.id = ids_to_unref[i];
      resource.count = counts_to_unref[i];
      unref_array.push_back(resource);
    }
    factory_->UnrefResources(unref_array);
  }

  void CheckReturnedResourcesMatchExpected(ResourceId* expected_returned_ids,
                                           int* expected_returned_counts,
                                           size_t expected_resources,
                                           gpu::SyncToken expected_sync_token) {
    const ReturnedResourceArray& actual_resources =
        fake_surface_resource_holder_client_.returned_resources();
    ASSERT_EQ(expected_resources, actual_resources.size());
    for (size_t i = 0; i < expected_resources; ++i) {
      ReturnedResource resource = actual_resources[i];
      EXPECT_EQ(expected_sync_token, resource.sync_token);
      EXPECT_EQ(expected_returned_ids[i], resource.id);
      EXPECT_EQ(expected_returned_counts[i], resource.count);
    }
    fake_surface_resource_holder_client_.clear_returned_resources();
  }

  void RefCurrentFrameResources() {
    Surface* surface = manager_.GetSurfaceForId(
        SurfaceId(factory_->frame_sink_id(), local_surface_id_));
    factory_->RefResources(surface->GetActiveFrame().resource_list);
  }

 protected:
  SurfaceManager manager_;
  StubSurfaceFactoryClient stub_surface_factory_client_;
  FakeSurfaceResourceHolderClient fake_surface_resource_holder_client_;
  std::unique_ptr<SurfaceFactory> factory_;
  LocalSurfaceId local_surface_id_;
  SurfaceId last_created_surface_id_;
  SurfaceInfo last_surface_info_;

  // This is the sync token submitted with the frame. It should never be
  // returned to the client.
  const gpu::SyncToken frame_sync_token_;

  // This is the sync token returned by the consumer. It should always be
  // returned to the client.
  const gpu::SyncToken consumer_sync_token_;
};

// Tests submitting a frame with resources followed by one with no resources
// with no resource provider action in between.
TEST_F(SurfaceFactoryTest, ResourceLifetimeSimple) {
  ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u,
            fake_surface_resource_holder_client_.returned_resources().size());
  fake_surface_resource_holder_client_.clear_returned_resources();

  // The second frame references no resources of first frame and thus should
  // make all resources of first frame available to be returned.
  SubmitCompositorFrameWithResources(NULL, 0);

  ResourceId expected_returned_ids[] = {1, 2, 3};
  int expected_returned_counts[] = {1, 1, 1};
  // Resources were never consumed so no sync token should be set.
  CheckReturnedResourcesMatchExpected(
      expected_returned_ids, expected_returned_counts,
      arraysize(expected_returned_counts), gpu::SyncToken());

  ResourceId third_frame_ids[] = {4, 5, 6};
  SubmitCompositorFrameWithResources(third_frame_ids,
                                     arraysize(third_frame_ids));

  // All of the resources submitted in the third frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u,
            fake_surface_resource_holder_client_.returned_resources().size());
  fake_surface_resource_holder_client_.clear_returned_resources();

  // The forth frame references no resources of third frame and thus should
  // make all resources of third frame available to be returned.
  ResourceId forth_frame_ids[] = {7, 8, 9};
  SubmitCompositorFrameWithResources(forth_frame_ids,
                                     arraysize(forth_frame_ids));

  ResourceId forth_expected_returned_ids[] = {4, 5, 6};
  int forth_expected_returned_counts[] = {1, 1, 1};
  // Resources were never consumed so no sync token should be set.
  CheckReturnedResourcesMatchExpected(
      forth_expected_returned_ids, forth_expected_returned_counts,
      arraysize(forth_expected_returned_counts), gpu::SyncToken());
}

// Tests submitting a frame with resources followed by one with no resources
// with the resource provider holding everything alive.
TEST_F(SurfaceFactoryTest, ResourceLifetimeSimpleWithProviderHoldingAlive) {
  ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u,
            fake_surface_resource_holder_client_.returned_resources().size());
  fake_surface_resource_holder_client_.clear_returned_resources();

  // Hold on to everything.
  RefCurrentFrameResources();

  // The second frame references no resources and thus should make all resources
  // available to be returned as soon as the resource provider releases them.
  SubmitCompositorFrameWithResources(NULL, 0);

  EXPECT_EQ(0u,
            fake_surface_resource_holder_client_.returned_resources().size());
  fake_surface_resource_holder_client_.clear_returned_resources();

  int release_counts[] = {1, 1, 1};
  UnrefResources(first_frame_ids, release_counts, arraysize(first_frame_ids));

  ResourceId expected_returned_ids[] = {1, 2, 3};
  int expected_returned_counts[] = {1, 1, 1};
  CheckReturnedResourcesMatchExpected(
      expected_returned_ids, expected_returned_counts,
      arraysize(expected_returned_counts), consumer_sync_token_);
}

// Tests referencing a resource, unref'ing it to zero, then using it again
// before returning it to the client.
TEST_F(SurfaceFactoryTest, ResourceReusedBeforeReturn) {
  ResourceId first_frame_ids[] = {7};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // This removes all references to resource id 7.
  SubmitCompositorFrameWithResources(NULL, 0);

  // This references id 7 again.
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // This removes it again.
  SubmitCompositorFrameWithResources(NULL, 0);

  // Now it should be returned.
  // We don't care how many entries are in the returned array for 7, so long as
  // the total returned count matches the submitted count.
  const ReturnedResourceArray& returned =
      fake_surface_resource_holder_client_.returned_resources();
  size_t return_count = 0;
  for (size_t i = 0; i < returned.size(); ++i) {
    EXPECT_EQ(7u, returned[i].id);
    return_count += returned[i].count;
  }
  EXPECT_EQ(2u, return_count);
}

// Tests having resources referenced multiple times, as if referenced by
// multiple providers.
TEST_F(SurfaceFactoryTest, ResourceRefMultipleTimes) {
  ResourceId first_frame_ids[] = {3, 4};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // Ref resources from the first frame twice.
  RefCurrentFrameResources();
  RefCurrentFrameResources();

  ResourceId second_frame_ids[] = {4, 5};
  SubmitCompositorFrameWithResources(second_frame_ids,
                                     arraysize(second_frame_ids));

  // Ref resources from the second frame 3 times.
  RefCurrentFrameResources();
  RefCurrentFrameResources();
  RefCurrentFrameResources();

  // Submit a frame with no resources to remove all current frame refs from
  // submitted resources.
  SubmitCompositorFrameWithResources(NULL, 0);

  EXPECT_EQ(0u,
            fake_surface_resource_holder_client_.returned_resources().size());
  fake_surface_resource_holder_client_.clear_returned_resources();

  // Expected current refs:
  //  3 -> 2
  //  4 -> 2 + 3 = 5
  //  5 -> 3
  {
    SCOPED_TRACE("unref all 3");
    ResourceId ids_to_unref[] = {3, 4, 5};
    int counts[] = {1, 1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));

    EXPECT_EQ(0u,
              fake_surface_resource_holder_client_.returned_resources().size());
    fake_surface_resource_holder_client_.clear_returned_resources();

    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));

    ResourceId expected_returned_ids[] = {3};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }

  // Expected refs remaining:
  //  4 -> 3
  //  5 -> 1
  {
    SCOPED_TRACE("unref 4 and 5");
    ResourceId ids_to_unref[] = {4, 5};
    int counts[] = {1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));

    ResourceId expected_returned_ids[] = {5};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }

  // Now, just 2 refs remaining on resource 4. Unref both at once and make sure
  // the returned count is correct.
  {
    SCOPED_TRACE("unref only 4");
    ResourceId ids_to_unref[] = {4};
    int counts[] = {2};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));

    ResourceId expected_returned_ids[] = {4};
    int expected_returned_counts[] = {2};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }
}

TEST_F(SurfaceFactoryTest, ResourceLifetime) {
  ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u,
            fake_surface_resource_holder_client_.returned_resources().size());
  fake_surface_resource_holder_client_.clear_returned_resources();

  // The second frame references some of the same resources, but some different
  // ones. We expect to receive back resource 1 with a count of 1 since it was
  // only referenced by the first frame.
  ResourceId second_frame_ids[] = {2, 3, 4};
  SubmitCompositorFrameWithResources(second_frame_ids,
                                     arraysize(second_frame_ids));

  {
    SCOPED_TRACE("second frame");
    ResourceId expected_returned_ids[] = {1};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), gpu::SyncToken());
  }

  // The third frame references a disjoint set of resources, so we expect to
  // receive back all resources from the first and second frames. Resource IDs 2
  // and 3 will have counts of 2, since they were used in both frames, and
  // resource ID 4 will have a count of 1.
  ResourceId third_frame_ids[] = {10, 11, 12, 13};
  SubmitCompositorFrameWithResources(third_frame_ids,
                                     arraysize(third_frame_ids));

  {
    SCOPED_TRACE("third frame");
    ResourceId expected_returned_ids[] = {2, 3, 4};
    int expected_returned_counts[] = {2, 2, 1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), gpu::SyncToken());
  }

  // Simulate a ResourceProvider taking a ref on all of the resources.
  RefCurrentFrameResources();

  ResourceId fourth_frame_ids[] = {12, 13};
  SubmitCompositorFrameWithResources(fourth_frame_ids,
                                     arraysize(fourth_frame_ids));

  EXPECT_EQ(0u,
            fake_surface_resource_holder_client_.returned_resources().size());

  RefCurrentFrameResources();

  // All resources are still being used by the external reference, so none can
  // be returned to the client.
  EXPECT_EQ(0u,
            fake_surface_resource_holder_client_.returned_resources().size());

  // Release resources associated with the first RefCurrentFrameResources() call
  // first.
  {
    ResourceId ids_to_unref[] = {10, 11, 12, 13};
    int counts[] = {1, 1, 1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));
  }

  {
    SCOPED_TRACE("fourth frame, first unref");
    ResourceId expected_returned_ids[] = {10, 11};
    int expected_returned_counts[] = {1, 1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }

  {
    ResourceId ids_to_unref[] = {12, 13};
    int counts[] = {1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));
  }

  // Resources 12 and 13 are still in use by the current frame, so they
  // shouldn't be available to be returned.
  EXPECT_EQ(0u,
            fake_surface_resource_holder_client_.returned_resources().size());

  // If we submit an empty frame, however, they should become available.
  SubmitCompositorFrameWithResources(NULL, 0u);

  {
    SCOPED_TRACE("fourth frame, second unref");
    ResourceId expected_returned_ids[] = {12, 13};
    int expected_returned_counts[] = {2, 2};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }
}

TEST_F(SurfaceFactoryTest, BlankNoIndexIncrement) {
  LocalSurfaceId local_surface_id(6, kArbitraryToken);
  SurfaceId surface_id(kArbitraryFrameSinkId, local_surface_id);
  factory_->SubmitCompositorFrame(local_surface_id, CompositorFrame(),
                                  SurfaceFactory::DrawCallback(),
                                  SurfaceFactory::WillDrawCallback());
  Surface* surface = manager_.GetSurfaceForId(surface_id);
  ASSERT_NE(nullptr, surface);
  EXPECT_EQ(2, surface->frame_index());
  EXPECT_EQ(last_created_surface_id().local_surface_id(), local_surface_id);
}

void CreateSurfaceDrawCallback(SurfaceFactory* factory,
                               uint32_t* execute_count) {
  LocalSurfaceId new_id(7, base::UnguessableToken::Create());
  factory->SubmitCompositorFrame(new_id, CompositorFrame(),
                                 SurfaceFactory::DrawCallback(),
                                 SurfaceFactory::WillDrawCallback());
  factory->EvictSurface();
  *execute_count += 1;
}

TEST_F(SurfaceFactoryTest, AddDuringEviction) {
  LocalSurfaceId local_surface_id(6, kArbitraryToken);

  uint32_t execute_count = 0;
  factory_->SubmitCompositorFrame(
      local_surface_id, CompositorFrame(),
      base::Bind(&CreateSurfaceDrawCallback, base::Unretained(factory_.get()),
                 &execute_count),
      SurfaceFactory::WillDrawCallback());
  EXPECT_EQ(0u, execute_count);
  factory_->EvictSurface();
  EXPECT_EQ(1u, execute_count);
}

void DrawCallback(uint32_t* execute_count) {
  *execute_count += 1;
}

// Tests doing an EvictSurface before shutting down the factory.
TEST_F(SurfaceFactoryTest, EvictSurface) {
  LocalSurfaceId local_surface_id(7, kArbitraryToken);
  SurfaceId id(kArbitraryFrameSinkId, local_surface_id);

  TransferableResource resource;
  resource.id = 1;
  resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
  CompositorFrame frame;
  frame.resource_list.push_back(resource);
  uint32_t execute_count = 0;
  factory_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                  base::Bind(&DrawCallback, &execute_count),
                                  SurfaceFactory::WillDrawCallback());
  EXPECT_EQ(last_created_surface_id().local_surface_id(), local_surface_id);
  local_surface_id_ = LocalSurfaceId();

  EXPECT_TRUE(manager_.GetSurfaceForId(id));
  EXPECT_TRUE(
      fake_surface_resource_holder_client_.returned_resources().empty());
  factory_->EvictSurface();
  EXPECT_FALSE(manager_.GetSurfaceForId(id));
  EXPECT_FALSE(
      fake_surface_resource_holder_client_.returned_resources().empty());
  EXPECT_EQ(1u, execute_count);
}

// Tests doing an EvictSurface which has unregistered dependency.
TEST_F(SurfaceFactoryTest, EvictSurfaceDependencyUnRegistered) {
  LocalSurfaceId local_surface_id(7, kArbitraryToken);

  TransferableResource resource;
  resource.id = 1;
  resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
  CompositorFrame frame;
  frame.resource_list.push_back(resource);
  uint32_t execute_count = 0;
  factory_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                  base::Bind(&DrawCallback, &execute_count),
                                  SurfaceFactory::WillDrawCallback());
  EXPECT_EQ(last_created_surface_id().local_surface_id(), local_surface_id);
  local_surface_id_ = LocalSurfaceId();

  SurfaceId surface_id(kArbitraryFrameSinkId, local_surface_id);
  Surface* surface = manager_.GetSurfaceForId(surface_id);
  surface->AddDestructionDependency(
      SurfaceSequence(kAnotherArbitraryFrameSinkId, 4));

  EXPECT_TRUE(manager_.GetSurfaceForId(surface_id));
  EXPECT_TRUE(
      fake_surface_resource_holder_client_.returned_resources().empty());
  factory_->EvictSurface();
  EXPECT_FALSE(manager_.GetSurfaceForId(surface_id));
  EXPECT_FALSE(
      fake_surface_resource_holder_client_.returned_resources().empty());
  EXPECT_EQ(1u, execute_count);
}

// Tests doing an EvictSurface which has registered dependency.
TEST_F(SurfaceFactoryTest, EvictSurfaceDependencyRegistered) {
  LocalSurfaceId local_surface_id(7, kArbitraryToken);

  TransferableResource resource;
  resource.id = 1;
  resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
  CompositorFrame frame;
  frame.resource_list.push_back(resource);
  uint32_t execute_count = 0;
  factory_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                  base::Bind(&DrawCallback, &execute_count),
                                  SurfaceFactory::WillDrawCallback());
  EXPECT_EQ(last_created_surface_id().local_surface_id(), local_surface_id);
  local_surface_id_ = LocalSurfaceId();

  manager_.RegisterFrameSinkId(kAnotherArbitraryFrameSinkId);

  SurfaceId surface_id(kArbitraryFrameSinkId, local_surface_id);
  Surface* surface = manager_.GetSurfaceForId(surface_id);
  surface->AddDestructionDependency(
      SurfaceSequence(kAnotherArbitraryFrameSinkId, 4));

  EXPECT_TRUE(manager_.GetSurfaceForId(surface_id));
  EXPECT_TRUE(
      fake_surface_resource_holder_client_.returned_resources().empty());
  factory_->EvictSurface();
  EXPECT_TRUE(manager_.GetSurfaceForId(surface_id));
  EXPECT_TRUE(
      fake_surface_resource_holder_client_.returned_resources().empty());
  EXPECT_EQ(0u, execute_count);

  manager_.SatisfySequence(SurfaceSequence(kAnotherArbitraryFrameSinkId, 4));
  EXPECT_FALSE(manager_.GetSurfaceForId(surface_id));
  EXPECT_FALSE(
      fake_surface_resource_holder_client_.returned_resources().empty());
}

TEST_F(SurfaceFactoryTest, DestroySequence) {
  LocalSurfaceId local_surface_id2(5, kArbitraryToken);
  std::unique_ptr<SurfaceFactory> factory2(new SurfaceFactory(
      kArbitraryFrameSinkId, &manager_, &stub_surface_factory_client_,
      &fake_surface_resource_holder_client_));
  SurfaceId id2(kArbitraryFrameSinkId, local_surface_id2);
  factory2->SubmitCompositorFrame(local_surface_id2, CompositorFrame(),
                                  SurfaceFactory::DrawCallback(),
                                  SurfaceFactory::WillDrawCallback());

  manager_.RegisterFrameSinkId(kArbitraryFrameSinkId);

  // Check that waiting before the sequence is satisfied works.
  manager_.GetSurfaceForId(id2)->AddDestructionDependency(
      SurfaceSequence(kArbitraryFrameSinkId, 4));
  factory2->EvictSurface();

  DCHECK(manager_.GetSurfaceForId(id2));
  manager_.SatisfySequence(SurfaceSequence(kArbitraryFrameSinkId, 4));
  manager_.SatisfySequence(SurfaceSequence(kArbitraryFrameSinkId, 6));
  DCHECK(!manager_.GetSurfaceForId(id2));

  // Check that waiting after the sequence is satisfied works.
  factory2->SubmitCompositorFrame(local_surface_id2, CompositorFrame(),
                                  SurfaceFactory::DrawCallback(),
                                  SurfaceFactory::WillDrawCallback());
  DCHECK(manager_.GetSurfaceForId(id2));
  manager_.GetSurfaceForId(id2)->AddDestructionDependency(
      SurfaceSequence(kAnotherArbitraryFrameSinkId, 6));
  factory2->EvictSurface();
  DCHECK(!manager_.GetSurfaceForId(id2));
}

// Tests that Surface ID namespace invalidation correctly allows
// Sequences to be ignored.
TEST_F(SurfaceFactoryTest, InvalidFrameSinkId) {
  FrameSinkId frame_sink_id(1234, 5678);

  LocalSurfaceId local_surface_id(5, kArbitraryToken);
  SurfaceId id(factory_->frame_sink_id(), local_surface_id);
  factory_->SubmitCompositorFrame(local_surface_id, CompositorFrame(),
                                  SurfaceFactory::DrawCallback(),
                                  SurfaceFactory::WillDrawCallback());

  manager_.RegisterFrameSinkId(frame_sink_id);
  manager_.GetSurfaceForId(id)->AddDestructionDependency(
      SurfaceSequence(frame_sink_id, 4));

  factory_->EvictSurface();

  // Verify the dependency has prevented the surface from getting destroyed.
  EXPECT_TRUE(manager_.GetSurfaceForId(id));

  manager_.InvalidateFrameSinkId(frame_sink_id);

  // Verify that the invalidated namespace caused the unsatisfied sequence
  // to be ignored.
  EXPECT_FALSE(manager_.GetSurfaceForId(id));
}

TEST_F(SurfaceFactoryTest, DestroyCycle) {
  LocalSurfaceId local_surface_id2(5, kArbitraryToken);
  SurfaceId id2(kArbitraryFrameSinkId, local_surface_id2);
  std::unique_ptr<SurfaceFactory> factory2(new SurfaceFactory(
      kArbitraryFrameSinkId, &manager_, &stub_surface_factory_client_,
      &fake_surface_resource_holder_client_));
  manager_.RegisterFrameSinkId(kAnotherArbitraryFrameSinkId);
  // Give id2 a frame that references local_surface_id_.
  {
    std::unique_ptr<RenderPass> render_pass(RenderPass::Create());
    CompositorFrame frame;
    frame.render_pass_list.push_back(std::move(render_pass));
    frame.metadata.referenced_surfaces.push_back(
        SurfaceId(factory_->frame_sink_id(), local_surface_id_));
    factory2->SubmitCompositorFrame(local_surface_id2, std::move(frame),
                                    SurfaceFactory::DrawCallback(),
                                    SurfaceFactory::WillDrawCallback());
    EXPECT_EQ(last_created_surface_id().local_surface_id(), local_surface_id2);
  }
  manager_.GetSurfaceForId(id2)->AddDestructionDependency(
      SurfaceSequence(kAnotherArbitraryFrameSinkId, 4));
  factory2->EvictSurface();
  // Give local_surface_id_ a frame that references id2.
  {
    std::unique_ptr<RenderPass> render_pass(RenderPass::Create());
    CompositorFrame frame;
    frame.render_pass_list.push_back(std::move(render_pass));
    frame.metadata.referenced_surfaces.push_back(id2);
    factory_->SubmitCompositorFrame(local_surface_id_, std::move(frame),
                                    SurfaceFactory::DrawCallback(),
                                    SurfaceFactory::WillDrawCallback());
  }
  factory_->EvictSurface();
  EXPECT_TRUE(manager_.GetSurfaceForId(id2));
  // local_surface_id_ should be retained by reference from id2.
  EXPECT_TRUE(manager_.GetSurfaceForId(
      SurfaceId(factory_->frame_sink_id(), local_surface_id_)));

  // Satisfy last destruction dependency for id2.
  manager_.SatisfySequence(SurfaceSequence(kAnotherArbitraryFrameSinkId, 4));

  // id2 and local_surface_id_ are in a reference cycle that has no surface
  // sequences holding on to it, so they should be destroyed.
  EXPECT_TRUE(!manager_.GetSurfaceForId(id2));
  EXPECT_TRUE(!manager_.GetSurfaceForId(
      SurfaceId(factory_->frame_sink_id(), local_surface_id_)));

  local_surface_id_ = LocalSurfaceId();
}

void CopyRequestTestCallback(bool* called,
                             std::unique_ptr<CopyOutputResult> result) {
  *called = true;
}

TEST_F(SurfaceFactoryTest, DuplicateCopyRequest) {
  {
    std::unique_ptr<RenderPass> render_pass(RenderPass::Create());
    CompositorFrame frame;
    frame.render_pass_list.push_back(std::move(render_pass));
    frame.metadata.referenced_surfaces.push_back(
        SurfaceId(factory_->frame_sink_id(), local_surface_id_));
    factory_->SubmitCompositorFrame(local_surface_id_, std::move(frame),
                                    SurfaceFactory::DrawCallback(),
                                    SurfaceFactory::WillDrawCallback());
    EXPECT_EQ(last_created_surface_id().local_surface_id(), local_surface_id_);
  }

  bool called1 = false;
  std::unique_ptr<CopyOutputRequest> request;
  request = CopyOutputRequest::CreateRequest(
      base::Bind(&CopyRequestTestCallback, &called1));
  request->set_source(kArbitrarySourceId1);

  factory_->RequestCopyOfSurface(std::move(request));
  EXPECT_FALSE(called1);

  bool called2 = false;
  request = CopyOutputRequest::CreateRequest(
      base::Bind(&CopyRequestTestCallback, &called2));
  request->set_source(kArbitrarySourceId2);

  factory_->RequestCopyOfSurface(std::move(request));
  // Callbacks have different sources so neither should be called.
  EXPECT_FALSE(called1);
  EXPECT_FALSE(called2);

  bool called3 = false;
  request = CopyOutputRequest::CreateRequest(
      base::Bind(&CopyRequestTestCallback, &called3));
  request->set_source(kArbitrarySourceId1);

  factory_->RequestCopyOfSurface(std::move(request));
  // Two callbacks are from source1, so the first should be called.
  EXPECT_TRUE(called1);
  EXPECT_FALSE(called2);
  EXPECT_FALSE(called3);

  factory_->EvictSurface();
  local_surface_id_ = LocalSurfaceId();
  EXPECT_TRUE(called1);
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called3);
}

// Check whether the SurfaceInfo object is created and populated correctly
// after the frame submission.
TEST_F(SurfaceFactoryTest, SurfaceInfo) {
  CompositorFrame frame;

  auto render_pass = RenderPass::Create();
  render_pass->SetNew(1, gfx::Rect(5, 6), gfx::Rect(), gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass));

  render_pass = RenderPass::Create();
  render_pass->SetNew(2, gfx::Rect(7, 8), gfx::Rect(), gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass));

  frame.metadata.device_scale_factor = 2.5f;

  factory_->SubmitCompositorFrame(local_surface_id_, std::move(frame),
                                  SurfaceFactory::DrawCallback(),
                                  SurfaceFactory::WillDrawCallback());
  SurfaceId expected_surface_id(factory_->frame_sink_id(), local_surface_id_);
  EXPECT_EQ(expected_surface_id, last_surface_info_.id());
  EXPECT_EQ(2.5f, last_surface_info_.device_scale_factor());
  EXPECT_EQ(gfx::Size(7, 8), last_surface_info_.size_in_pixels());
}

}  // namespace
}  // namespace cc
