// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"

#include "base/macros.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/compositor_frame_helpers.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "cc/test/fake_surface_observer.h"
#include "cc/test/mock_compositor_frame_sink_support_client.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;
using testing::IsEmpty;
using testing::SizeIs;
using testing::Invoke;
using testing::_;
using testing::Eq;

namespace viz {
namespace {

constexpr bool kIsRoot = true;
constexpr bool kIsChildRoot = false;
constexpr bool kHandlesFrameSinkIdInvalidation = true;
constexpr bool kNeedsSyncPoints = true;

constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);
constexpr FrameSinkId kAnotherArbitraryFrameSinkId(2, 2);
constexpr FrameSinkId kYetAnotherArbitraryFrameSinkId(3, 3);

const base::UnguessableToken kArbitraryToken = base::UnguessableToken::Create();
const base::UnguessableToken kArbitrarySourceId1 =
    base::UnguessableToken::Deserialize(0xdead, 0xbeef);
const base::UnguessableToken kArbitrarySourceId2 =
    base::UnguessableToken::Deserialize(0xdead, 0xbee0);

gpu::SyncToken GenTestSyncToken(int id) {
  gpu::SyncToken token;
  token.Set(gpu::CommandBufferNamespace::GPU_IO, 0,
            gpu::CommandBufferId::FromUnsafeValue(id), 1);
  return token;
}

class FakeCompositorFrameSinkSupportClient
    : public CompositorFrameSinkSupportClient {
 public:
  FakeCompositorFrameSinkSupportClient() = default;
  ~FakeCompositorFrameSinkSupportClient() override = default;

  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override {
    InsertResources(resources);
  }

  void OnBeginFrame(const cc::BeginFrameArgs& args) override {}

  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override {
    InsertResources(resources);
  }

  void WillDrawSurface(const LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override {}

  void clear_returned_resources() { returned_resources_.clear(); }
  const std::vector<cc::ReturnedResource>& returned_resources() {
    return returned_resources_;
  }

 private:
  void InsertResources(const std::vector<cc::ReturnedResource>& resources) {
    returned_resources_.insert(returned_resources_.end(), resources.begin(),
                               resources.end());
  }

  std::vector<cc::ReturnedResource> returned_resources_;

  DISALLOW_COPY_AND_ASSIGN(FakeCompositorFrameSinkSupportClient);
};

class CompositorFrameSinkSupportTest : public testing::Test {
 public:
  CompositorFrameSinkSupportTest()
      : support_(
            CompositorFrameSinkSupport::Create(&fake_support_client_,
                                               &manager_,
                                               kArbitraryFrameSinkId,
                                               kIsRoot,
                                               kHandlesFrameSinkIdInvalidation,
                                               kNeedsSyncPoints)),
        begin_frame_source_(0.f, false),
        local_surface_id_(3, kArbitraryToken),
        frame_sync_token_(GenTestSyncToken(4)),
        consumer_sync_token_(GenTestSyncToken(5)) {
    manager_.surface_manager()->AddObserver(&surface_observer_);
    support_->SetBeginFrameSource(&begin_frame_source_);
  }
  ~CompositorFrameSinkSupportTest() override {
    manager_.surface_manager()->RemoveObserver(&surface_observer_);
    support_->EvictCurrentSurface();
  }

  void SubmitCompositorFrameWithResources(cc::ResourceId* resource_ids,
                                          size_t num_resource_ids) {
    auto frame = cc::test::MakeCompositorFrame();
    for (size_t i = 0u; i < num_resource_ids; ++i) {
      cc::TransferableResource resource;
      resource.id = resource_ids[i];
      resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
      resource.mailbox_holder.sync_token = frame_sync_token_;
      frame.resource_list.push_back(resource);
    }
    support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
    EXPECT_EQ(surface_observer_.last_created_surface_id().local_surface_id(),
              local_surface_id_);
  }

  void UnrefResources(cc::ResourceId* ids_to_unref,
                      int* counts_to_unref,
                      size_t num_ids_to_unref) {
    std::vector<cc::ReturnedResource> unref_array;
    for (size_t i = 0; i < num_ids_to_unref; ++i) {
      cc::ReturnedResource resource;
      resource.sync_token = consumer_sync_token_;
      resource.id = ids_to_unref[i];
      resource.count = counts_to_unref[i];
      unref_array.push_back(resource);
    }
    support_->UnrefResources(unref_array);
  }

  void CheckReturnedResourcesMatchExpected(
      cc::ResourceId* expected_returned_ids,
      int* expected_returned_counts,
      size_t expected_resources,
      gpu::SyncToken expected_sync_token) {
    const std::vector<cc::ReturnedResource>& actual_resources =
        fake_support_client_.returned_resources();
    ASSERT_EQ(expected_resources, actual_resources.size());
    for (size_t i = 0; i < expected_resources; ++i) {
      cc::ReturnedResource resource = actual_resources[i];
      EXPECT_EQ(expected_sync_token, resource.sync_token);
      EXPECT_EQ(expected_returned_ids[i], resource.id);
      EXPECT_EQ(expected_returned_counts[i], resource.count);
    }
    fake_support_client_.clear_returned_resources();
  }

  cc::Surface* GetSurfaceForId(const SurfaceId& id) {
    return manager_.surface_manager()->GetSurfaceForId(id);
  }

  void RefCurrentFrameResources() {
    cc::Surface* surface = GetSurfaceForId(
        SurfaceId(support_->frame_sink_id(), local_surface_id_));
    support_->RefResources(surface->GetActiveFrame().resource_list);
  }

 protected:
  FrameSinkManagerImpl manager_;
  FakeCompositorFrameSinkSupportClient fake_support_client_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  cc::FakeExternalBeginFrameSource begin_frame_source_;
  LocalSurfaceId local_surface_id_;
  cc::FakeSurfaceObserver surface_observer_;

  // This is the sync token submitted with the frame. It should never be
  // returned to the client.
  const gpu::SyncToken frame_sync_token_;

  // This is the sync token returned by the consumer. It should always be
  // returned to the client.
  const gpu::SyncToken consumer_sync_token_;
};

// Tests submitting a frame with resources followed by one with no resources
// with no resource provider action in between.
TEST_F(CompositorFrameSinkSupportTest, ResourceLifetimeSimple) {
  cc::ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  // The second frame references no resources of first frame and thus should
  // make all resources of first frame available to be returned.
  SubmitCompositorFrameWithResources(NULL, 0);

  cc::ResourceId expected_returned_ids[] = {1, 2, 3};
  int expected_returned_counts[] = {1, 1, 1};
  // Resources were never consumed so no sync token should be set.
  CheckReturnedResourcesMatchExpected(
      expected_returned_ids, expected_returned_counts,
      arraysize(expected_returned_counts), gpu::SyncToken());

  cc::ResourceId third_frame_ids[] = {4, 5, 6};
  SubmitCompositorFrameWithResources(third_frame_ids,
                                     arraysize(third_frame_ids));

  // All of the resources submitted in the third frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  // The forth frame references no resources of third frame and thus should
  // make all resources of third frame available to be returned.
  cc::ResourceId forth_frame_ids[] = {7, 8, 9};
  SubmitCompositorFrameWithResources(forth_frame_ids,
                                     arraysize(forth_frame_ids));

  cc::ResourceId forth_expected_returned_ids[] = {4, 5, 6};
  int forth_expected_returned_counts[] = {1, 1, 1};
  // Resources were never consumed so no sync token should be set.
  CheckReturnedResourcesMatchExpected(
      forth_expected_returned_ids, forth_expected_returned_counts,
      arraysize(forth_expected_returned_counts), gpu::SyncToken());
}

// Tests submitting a frame with resources followed by one with no resources
// with the resource provider holding everything alive.
TEST_F(CompositorFrameSinkSupportTest,
       ResourceLifetimeSimpleWithProviderHoldingAlive) {
  cc::ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  // Hold on to everything.
  RefCurrentFrameResources();

  // The second frame references no resources and thus should make all resources
  // available to be returned as soon as the resource provider releases them.
  SubmitCompositorFrameWithResources(NULL, 0);

  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  int release_counts[] = {1, 1, 1};
  UnrefResources(first_frame_ids, release_counts, arraysize(first_frame_ids));

  // None is returned to the client since DidReceiveCompositorAck is not
  // invoked.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());

  // Submitting an empty frame causes previous resources referenced by the
  // previous frame to be returned to client.
  SubmitCompositorFrameWithResources(nullptr, 0);
  cc::ResourceId expected_returned_ids[] = {1, 2, 3};
  int expected_returned_counts[] = {1, 1, 1};
  CheckReturnedResourcesMatchExpected(
      expected_returned_ids, expected_returned_counts,
      arraysize(expected_returned_counts), consumer_sync_token_);
}

// Tests referencing a resource, unref'ing it to zero, then using it again
// before returning it to the client.
TEST_F(CompositorFrameSinkSupportTest, ResourceReusedBeforeReturn) {
  cc::ResourceId first_frame_ids[] = {7};
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
  const std::vector<cc::ReturnedResource>& returned =
      fake_support_client_.returned_resources();
  size_t return_count = 0;
  for (size_t i = 0; i < returned.size(); ++i) {
    EXPECT_EQ(7u, returned[i].id);
    return_count += returned[i].count;
  }
  EXPECT_EQ(2u, return_count);
}

// Tests having resources referenced multiple times, as if referenced by
// multiple providers.
TEST_F(CompositorFrameSinkSupportTest, ResourceRefMultipleTimes) {
  cc::ResourceId first_frame_ids[] = {3, 4};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // Ref resources from the first frame twice.
  RefCurrentFrameResources();
  RefCurrentFrameResources();

  cc::ResourceId second_frame_ids[] = {4, 5};
  SubmitCompositorFrameWithResources(second_frame_ids,
                                     arraysize(second_frame_ids));

  // Ref resources from the second frame 3 times.
  RefCurrentFrameResources();
  RefCurrentFrameResources();
  RefCurrentFrameResources();

  // Submit a frame with no resources to remove all current frame refs from
  // submitted resources.
  SubmitCompositorFrameWithResources(NULL, 0);

  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  // Expected current refs:
  //  3 -> 2
  //  4 -> 2 + 3 = 5
  //  5 -> 3
  {
    SCOPED_TRACE("unref all 3");
    cc::ResourceId ids_to_unref[] = {3, 4, 5};
    int counts[] = {1, 1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));

    EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
    fake_support_client_.clear_returned_resources();

    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));
    SubmitCompositorFrameWithResources(nullptr, 0);
    cc::ResourceId expected_returned_ids[] = {3};
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
    cc::ResourceId ids_to_unref[] = {4, 5};
    int counts[] = {1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));
    SubmitCompositorFrameWithResources(nullptr, 0);

    cc::ResourceId expected_returned_ids[] = {5};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }

  // Now, just 2 refs remaining on resource 4. Unref both at once and make sure
  // the returned count is correct.
  {
    SCOPED_TRACE("unref only 4");
    cc::ResourceId ids_to_unref[] = {4};
    int counts[] = {2};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));
    SubmitCompositorFrameWithResources(nullptr, 0);

    cc::ResourceId expected_returned_ids[] = {4};
    int expected_returned_counts[] = {2};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }
}

TEST_F(CompositorFrameSinkSupportTest, ResourceLifetime) {
  cc::ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  // The second frame references some of the same resources, but some different
  // ones. We expect to receive back resource 1 with a count of 1 since it was
  // only referenced by the first frame.
  cc::ResourceId second_frame_ids[] = {2, 3, 4};
  SubmitCompositorFrameWithResources(second_frame_ids,
                                     arraysize(second_frame_ids));
  {
    SCOPED_TRACE("second frame");
    cc::ResourceId expected_returned_ids[] = {1};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), gpu::SyncToken());
  }

  // The third frame references a disjoint set of resources, so we expect to
  // receive back all resources from the first and second frames. Resource IDs 2
  // and 3 will have counts of 2, since they were used in both frames, and
  // resource ID 4 will have a count of 1.
  cc::ResourceId third_frame_ids[] = {10, 11, 12, 13};
  SubmitCompositorFrameWithResources(third_frame_ids,
                                     arraysize(third_frame_ids));

  {
    SCOPED_TRACE("third frame");
    cc::ResourceId expected_returned_ids[] = {2, 3, 4};
    int expected_returned_counts[] = {2, 2, 1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), gpu::SyncToken());
  }

  // Simulate a ResourceProvider taking a ref on all of the resources.
  RefCurrentFrameResources();

  cc::ResourceId fourth_frame_ids[] = {12, 13};
  SubmitCompositorFrameWithResources(fourth_frame_ids,
                                     arraysize(fourth_frame_ids));

  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());

  RefCurrentFrameResources();

  // All resources are still being used by the external reference, so none can
  // be returned to the client.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());

  // Release resources associated with the first RefCurrentFrameResources() call
  // first.
  {
    cc::ResourceId ids_to_unref[] = {10, 11, 12, 13};
    int counts[] = {1, 1, 1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));
  }

  // Nothing is returned to the client yet since DidReceiveCompositorFrameAck
  // is not invoked.
  {
    SCOPED_TRACE("fourth frame, first unref");
    CheckReturnedResourcesMatchExpected(nullptr, nullptr, 0,
                                        consumer_sync_token_);
  }

  {
    cc::ResourceId ids_to_unref[] = {12, 13};
    int counts[] = {1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));
  }

  // Resources 12 and 13 are still in use by the current frame, so they
  // shouldn't be available to be returned.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());

  // If we submit an empty frame, however, they should become available.
  // Resources that were previously unref'd also return at this point.
  SubmitCompositorFrameWithResources(NULL, 0u);

  {
    SCOPED_TRACE("fourth frame, second unref");
    cc::ResourceId expected_returned_ids[] = {10, 11, 12, 13};
    int expected_returned_counts[] = {1, 1, 2, 2};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }
}

TEST_F(CompositorFrameSinkSupportTest, AddDuringEviction) {
  cc::test::MockCompositorFrameSinkSupportClient mock_client;
  auto support = CompositorFrameSinkSupport::Create(
      &mock_client, &manager_, kAnotherArbitraryFrameSinkId, kIsRoot,
      kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints);
  LocalSurfaceId local_surface_id(6, kArbitraryToken);
  support->SubmitCompositorFrame(local_surface_id,
                                 cc::test::MakeCompositorFrame());

  EXPECT_CALL(mock_client, DidReceiveCompositorFrameAck(_))
      .WillOnce(testing::InvokeWithoutArgs([&support, &mock_client]() {
        LocalSurfaceId new_id(7, base::UnguessableToken::Create());
        support->SubmitCompositorFrame(new_id, cc::test::MakeCompositorFrame());
      }))
      .WillRepeatedly(testing::Return());
  support->EvictCurrentSurface();
}

// Tests doing an EvictCurrentSurface before shutting down the factory.
TEST_F(CompositorFrameSinkSupportTest, EvictCurrentSurface) {
  cc::test::MockCompositorFrameSinkSupportClient mock_client;
  auto support = CompositorFrameSinkSupport::Create(
      &mock_client, &manager_, kAnotherArbitraryFrameSinkId, kIsRoot,
      kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints);
  LocalSurfaceId local_surface_id(7, kArbitraryToken);
  SurfaceId id(kAnotherArbitraryFrameSinkId, local_surface_id);

  cc::TransferableResource resource;
  resource.id = 1;
  resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
  auto frame = cc::test::MakeCompositorFrame();
  frame.resource_list.push_back(resource);
  support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  EXPECT_EQ(surface_observer_.last_created_surface_id().local_surface_id(),
            local_surface_id);
  local_surface_id_ = LocalSurfaceId();

  std::vector<cc::ReturnedResource> returned_resources = {
      resource.ToReturnedResource()};
  EXPECT_TRUE(GetSurfaceForId(id));
  EXPECT_CALL(mock_client, DidReceiveCompositorFrameAck(returned_resources))
      .Times(1);
  support->EvictCurrentSurface();
  EXPECT_FALSE(GetSurfaceForId(id));
}

// Tests doing an EvictCurrentSurface which has unregistered dependency.
TEST_F(CompositorFrameSinkSupportTest,
       EvictCurrentSurfaceDependencyUnRegistered) {
  cc::test::MockCompositorFrameSinkSupportClient mock_client;
  auto support = CompositorFrameSinkSupport::Create(
      &mock_client, &manager_, kAnotherArbitraryFrameSinkId, kIsRoot,
      kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints);
  LocalSurfaceId local_surface_id(7, kArbitraryToken);

  cc::TransferableResource resource;
  resource.id = 1;
  resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
  auto frame = cc::test::MakeCompositorFrame();
  frame.resource_list.push_back(resource);
  support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  EXPECT_EQ(surface_observer_.last_created_surface_id().local_surface_id(),
            local_surface_id);
  local_surface_id_ = LocalSurfaceId();

  SurfaceId surface_id(kAnotherArbitraryFrameSinkId, local_surface_id);
  cc::Surface* surface = GetSurfaceForId(surface_id);
  surface->AddDestructionDependency(
      SurfaceSequence(kYetAnotherArbitraryFrameSinkId, 4));

  std::vector<cc::ReturnedResource> returned_resource = {
      resource.ToReturnedResource()};

  EXPECT_TRUE(GetSurfaceForId(surface_id));
  EXPECT_CALL(mock_client, DidReceiveCompositorFrameAck(returned_resource))
      .Times(1);
  support->EvictCurrentSurface();
  EXPECT_FALSE(GetSurfaceForId(surface_id));
}

// Tests doing an EvictCurrentSurface which has registered dependency.
TEST_F(CompositorFrameSinkSupportTest,
       EvictCurrentSurfaceDependencyRegistered) {
  cc::test::MockCompositorFrameSinkSupportClient mock_client;
  auto support = CompositorFrameSinkSupport::Create(
      &mock_client, &manager_, kAnotherArbitraryFrameSinkId, kIsRoot,
      kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints);
  LocalSurfaceId local_surface_id(7, kArbitraryToken);

  cc::TransferableResource resource;
  resource.id = 1;
  resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
  auto frame = cc::test::MakeCompositorFrame();
  frame.resource_list.push_back(resource);
  uint32_t execute_count = 0;
  support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  EXPECT_EQ(surface_observer_.last_created_surface_id().local_surface_id(),
            local_surface_id);
  local_surface_id_ = LocalSurfaceId();

  manager_.surface_manager()->RegisterFrameSinkId(
      kYetAnotherArbitraryFrameSinkId);

  SurfaceId surface_id(kAnotherArbitraryFrameSinkId, local_surface_id);
  cc::Surface* surface = GetSurfaceForId(surface_id);
  surface->AddDestructionDependency(
      SurfaceSequence(kYetAnotherArbitraryFrameSinkId, 4));

  std::vector<cc::ReturnedResource> returned_resources;
  EXPECT_TRUE(GetSurfaceForId(surface_id));
  support->EvictCurrentSurface();
  EXPECT_TRUE(GetSurfaceForId(surface_id));
  EXPECT_EQ(0u, execute_count);

  returned_resources.push_back(resource.ToReturnedResource());
  EXPECT_CALL(mock_client, DidReceiveCompositorFrameAck(returned_resources))
      .Times(1);
  manager_.surface_manager()->SatisfySequence(
      SurfaceSequence(kYetAnotherArbitraryFrameSinkId, 4));
  EXPECT_FALSE(GetSurfaceForId(surface_id));
}

TEST_F(CompositorFrameSinkSupportTest, DestroySequence) {
  LocalSurfaceId local_surface_id2(5, kArbitraryToken);
  auto support2 = CompositorFrameSinkSupport::Create(
      &fake_support_client_, &manager_, kYetAnotherArbitraryFrameSinkId,
      kIsChildRoot, kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints);
  SurfaceId id2(kYetAnotherArbitraryFrameSinkId, local_surface_id2);
  support2->SubmitCompositorFrame(local_surface_id2,
                                  cc::test::MakeCompositorFrame());

  // Check that waiting before the sequence is satisfied works.
  GetSurfaceForId(id2)->AddDestructionDependency(
      SurfaceSequence(kYetAnotherArbitraryFrameSinkId, 4));
  support2->EvictCurrentSurface();

  DCHECK(GetSurfaceForId(id2));
  manager_.surface_manager()->SatisfySequence(
      SurfaceSequence(kYetAnotherArbitraryFrameSinkId, 4));
  manager_.surface_manager()->SatisfySequence(
      SurfaceSequence(kYetAnotherArbitraryFrameSinkId, 6));
  DCHECK(!GetSurfaceForId(id2));

  // Check that waiting after the sequence is satisfied works.
  support2->SubmitCompositorFrame(local_surface_id2,
                                  cc::test::MakeCompositorFrame());
  DCHECK(GetSurfaceForId(id2));
  GetSurfaceForId(id2)->AddDestructionDependency(
      SurfaceSequence(kAnotherArbitraryFrameSinkId, 6));
  support2->EvictCurrentSurface();
  DCHECK(!GetSurfaceForId(id2));
}

// Tests that SurfaceId namespace invalidation correctly allows
// Sequences to be ignored.
TEST_F(CompositorFrameSinkSupportTest, InvalidFrameSinkId) {
  FrameSinkId frame_sink_id(1234, 5678);

  LocalSurfaceId local_surface_id(5, kArbitraryToken);
  SurfaceId id(support_->frame_sink_id(), local_surface_id);
  support_->SubmitCompositorFrame(local_surface_id,
                                  cc::test::MakeCompositorFrame());

  manager_.surface_manager()->RegisterFrameSinkId(frame_sink_id);
  GetSurfaceForId(id)->AddDestructionDependency(
      SurfaceSequence(frame_sink_id, 4));

  support_->EvictCurrentSurface();

  // Verify the dependency has prevented the surface from getting destroyed.
  EXPECT_TRUE(GetSurfaceForId(id));

  manager_.surface_manager()->InvalidateFrameSinkId(frame_sink_id);

  // Verify that the invalidated namespace caused the unsatisfied sequence
  // to be ignored.
  EXPECT_FALSE(GetSurfaceForId(id));
}

TEST_F(CompositorFrameSinkSupportTest, DestroyCycle) {
  LocalSurfaceId local_surface_id2(5, kArbitraryToken);
  SurfaceId id2(kYetAnotherArbitraryFrameSinkId, local_surface_id2);
  auto support2 = CompositorFrameSinkSupport::Create(
      &fake_support_client_, &manager_, kYetAnotherArbitraryFrameSinkId,
      kIsChildRoot, kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints);
  manager_.surface_manager()->RegisterFrameSinkId(kAnotherArbitraryFrameSinkId);
  // Give local_surface_id_ an initial frame so another client can refer to
  // that surface.
  {
    auto frame = cc::test::MakeCompositorFrame();
    support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  }
  // Give id2 a frame that references local_surface_id_.
  {
    auto frame = cc::test::MakeCompositorFrame();
    frame.metadata.referenced_surfaces.push_back(
        SurfaceId(support_->frame_sink_id(), local_surface_id_));
    support2->SubmitCompositorFrame(local_surface_id2, std::move(frame));
    EXPECT_EQ(surface_observer_.last_created_surface_id().local_surface_id(),
              local_surface_id2);
  }
  GetSurfaceForId(id2)->AddDestructionDependency(
      SurfaceSequence(kAnotherArbitraryFrameSinkId, 4));
  support2->EvictCurrentSurface();
  // Give local_surface_id_ a frame that references id2.
  {
    auto frame = cc::test::MakeCompositorFrame();
    frame.metadata.referenced_surfaces.push_back(id2);
    support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  }
  support_->EvictCurrentSurface();
  EXPECT_TRUE(GetSurfaceForId(id2));
  // local_surface_id_ should be retained by reference from id2.
  EXPECT_TRUE(
      GetSurfaceForId(SurfaceId(support_->frame_sink_id(), local_surface_id_)));

  // Satisfy last destruction dependency for id2.
  manager_.surface_manager()->SatisfySequence(
      SurfaceSequence(kAnotherArbitraryFrameSinkId, 4));

  // id2 and local_surface_id_ are in a reference cycle that has no surface
  // sequences holding on to it, so they should be destroyed.
  EXPECT_TRUE(!GetSurfaceForId(id2));
  EXPECT_TRUE(!GetSurfaceForId(
      SurfaceId(support_->frame_sink_id(), local_surface_id_)));

  local_surface_id_ = LocalSurfaceId();
}

void CopyRequestTestCallback(bool* called,
                             std::unique_ptr<cc::CopyOutputResult> result) {
  *called = true;
}

TEST_F(CompositorFrameSinkSupportTest, DuplicateCopyRequest) {
  {
    auto frame = cc::test::MakeCompositorFrame();
    frame.metadata.referenced_surfaces.push_back(
        SurfaceId(support_->frame_sink_id(), local_surface_id_));
    support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
    EXPECT_EQ(surface_observer_.last_created_surface_id().local_surface_id(),
              local_surface_id_);
  }

  bool called1 = false;
  auto request = cc::CopyOutputRequest::CreateRequest(
      base::BindOnce(&CopyRequestTestCallback, &called1));
  request->set_source(kArbitrarySourceId1);

  support_->RequestCopyOfSurface(std::move(request));
  EXPECT_FALSE(called1);

  bool called2 = false;
  request = cc::CopyOutputRequest::CreateRequest(
      base::BindOnce(&CopyRequestTestCallback, &called2));
  request->set_source(kArbitrarySourceId2);

  support_->RequestCopyOfSurface(std::move(request));
  // Callbacks have different sources so neither should be called.
  EXPECT_FALSE(called1);
  EXPECT_FALSE(called2);

  bool called3 = false;
  request = cc::CopyOutputRequest::CreateRequest(
      base::BindOnce(&CopyRequestTestCallback, &called3));
  request->set_source(kArbitrarySourceId1);

  support_->RequestCopyOfSurface(std::move(request));
  // Two callbacks are from source1, so the first should be called.
  EXPECT_TRUE(called1);
  EXPECT_FALSE(called2);
  EXPECT_FALSE(called3);

  support_->EvictCurrentSurface();
  local_surface_id_ = LocalSurfaceId();
  EXPECT_TRUE(called1);
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called3);
}

// Check whether the SurfaceInfo object is created and populated correctly
// after the frame submission.
TEST_F(CompositorFrameSinkSupportTest, SurfaceInfo) {
  auto frame = cc::test::MakeCompositorFrame();

  auto render_pass = cc::RenderPass::Create();
  render_pass->SetNew(1, gfx::Rect(5, 6), gfx::Rect(), gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass));

  render_pass = cc::RenderPass::Create();
  render_pass->SetNew(2, gfx::Rect(7, 8), gfx::Rect(), gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass));

  frame.metadata.device_scale_factor = 2.5f;

  support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  SurfaceId expected_surface_id(support_->frame_sink_id(), local_surface_id_);
  EXPECT_EQ(expected_surface_id, surface_observer_.last_surface_info().id());
  EXPECT_EQ(2.5f, surface_observer_.last_surface_info().device_scale_factor());
  EXPECT_EQ(gfx::Size(7, 8),
            surface_observer_.last_surface_info().size_in_pixels());
}

// Check that if a CompositorFrame is received with size zero, we don't create
// a Surface for it.
TEST_F(CompositorFrameSinkSupportTest, ZeroFrameSize) {
  SurfaceId id(support_->frame_sink_id(), local_surface_id_);
  auto frame = cc::test::MakeEmptyCompositorFrame();
  frame.render_pass_list.push_back(cc::RenderPass::Create());
  EXPECT_TRUE(
      support_->SubmitCompositorFrame(local_surface_id_, std::move(frame)));
  EXPECT_FALSE(GetSurfaceForId(id));
}

// Check that if a CompositorFrame is received with device scale factor of 0, we
// don't create a Surface for it.
TEST_F(CompositorFrameSinkSupportTest, ZeroDeviceScaleFactor) {
  SurfaceId id(support_->frame_sink_id(), local_surface_id_);
  auto frame = cc::test::MakeCompositorFrame();
  frame.metadata.device_scale_factor = 0.f;
  EXPECT_TRUE(
      support_->SubmitCompositorFrame(local_surface_id_, std::move(frame)));
  EXPECT_FALSE(GetSurfaceForId(id));
}

// Check that if the size of a CompositorFrame doesn't match the size of the
// Surface it's being submitted to, we skip the frame.
TEST_F(CompositorFrameSinkSupportTest, FrameSizeMismatch) {
  SurfaceId id(support_->frame_sink_id(), local_surface_id_);

  // Submit a frame with size (5,5).
  auto frame = cc::test::MakeEmptyCompositorFrame();
  auto pass = cc::RenderPass::Create();
  pass->SetNew(1, gfx::Rect(5, 5), gfx::Rect(), gfx::Transform());
  frame.render_pass_list.push_back(std::move(pass));
  EXPECT_TRUE(
      support_->SubmitCompositorFrame(local_surface_id_, std::move(frame)));
  EXPECT_TRUE(GetSurfaceForId(id));

  // Submit a frame with size (5,4). This frame should be rejected and the
  // surface should be destroyed.
  frame = cc::test::MakeEmptyCompositorFrame();
  pass = cc::RenderPass::Create();
  pass->SetNew(1, gfx::Rect(5, 4), gfx::Rect(), gfx::Transform());
  frame.render_pass_list.push_back(std::move(pass));
  EXPECT_FALSE(
      support_->SubmitCompositorFrame(local_surface_id_, std::move(frame)));
  EXPECT_FALSE(GetSurfaceForId(id));
}

// Check that if the device scale factor of a CompositorFrame doesn't match the
// device scale factor of the Surface it's being submitted to, the frame is
// rejected and the surface is destroyed.
TEST_F(CompositorFrameSinkSupportTest, DeviceScaleFactorMismatch) {
  SurfaceId id(support_->frame_sink_id(), local_surface_id_);

  // Submit a frame with device scale factor of 0.5.
  auto frame = cc::test::MakeCompositorFrame();
  frame.metadata.device_scale_factor = 0.5f;
  EXPECT_TRUE(
      support_->SubmitCompositorFrame(local_surface_id_, std::move(frame)));
  EXPECT_TRUE(GetSurfaceForId(id));

  // Submit a frame with device scale factor of 0.4. This frame should be
  // rejected and the surface should be destroyed.
  frame = cc::test::MakeCompositorFrame();
  frame.metadata.device_scale_factor = 0.4f;
  EXPECT_FALSE(
      support_->SubmitCompositorFrame(local_surface_id_, std::move(frame)));
  EXPECT_FALSE(GetSurfaceForId(id));
}

TEST_F(CompositorFrameSinkSupportTest, PassesOnBeginFrameAcks) {
  // Request BeginFrames.
  support_->SetNeedsBeginFrame(true);

  // Issue a BeginFrame.
  cc::BeginFrameArgs args =
      cc::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  begin_frame_source_.TestOnBeginFrame(args);

  // Check that the support and SurfaceManager forward the BeginFrameAck
  // attached to a CompositorFrame to the SurfaceObserver.
  cc::BeginFrameAck ack(0, 1, true);
  auto frame = cc::test::MakeCompositorFrame();
  frame.metadata.begin_frame_ack = ack;
  support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  EXPECT_EQ(ack, surface_observer_.last_ack());

  // Issue another BeginFrame.
  args = cc::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  begin_frame_source_.TestOnBeginFrame(args);

  // Check that the support and SurfaceManager forward a DidNotProduceFrame ack
  // to the SurfaceObserver.
  cc::BeginFrameAck ack2(0, 2, false);
  support_->DidNotProduceFrame(ack2);
  EXPECT_EQ(ack2, surface_observer_.last_ack());

  support_->SetNeedsBeginFrame(false);
}

}  // namespace
}  // namespace viz
