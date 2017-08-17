// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_frame_sink_manager.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace viz {
namespace test {
namespace {

constexpr FrameSinkId kFrameSinkParent1(1, 1);
constexpr FrameSinkId kFrameSinkParent2(2, 1);
constexpr FrameSinkId kFrameSinkChild1(3, 1);

// Makes a SurfaceId with a default nonce.
SurfaceId MakeSurfaceId(const FrameSinkId& frame_sink_id, uint32_t local_id) {
  return SurfaceId(
      frame_sink_id,
      LocalSurfaceId(local_id, base::UnguessableToken::Deserialize(0, 1u)));
}

// Makes a SurfaceInfo with a default device_scale_factor and size.
SurfaceInfo MakeSurfaceInfo(const SurfaceId& surface_id) {
  return SurfaceInfo(surface_id, 1.f, gfx::Size(1, 1));
}

// A fake (do-nothing) implementation of HostFrameSinkClient.
class FakeHostFrameSinkClient : public HostFrameSinkClient {
 public:
  FakeHostFrameSinkClient() = default;
  ~FakeHostFrameSinkClient() override = default;

  // HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeHostFrameSinkClient);
};

// A mock implementation of mojom::FrameSinkManager.
class MockFrameSinkManagerImpl : public FrameSinkManagerImpl {
 public:
  MockFrameSinkManagerImpl()
      : FrameSinkManagerImpl(nullptr,
                             SurfaceManager::LifetimeType::REFERENCES) {}
  ~MockFrameSinkManagerImpl() override = default;

  // mojom::FrameSinkManager:
  MOCK_METHOD1(RegisterFrameSinkId, void(const FrameSinkId& frame_sink_id));
  MOCK_METHOD1(InvalidateFrameSinkId, void(const FrameSinkId& frame_sink_id));
  // Work around for gmock not supporting move-only types.
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      mojom::CompositorFrameSinkRequest request,
      mojom::CompositorFrameSinkClientPtr client) override {
    MockCreateCompositorFrameSink(frame_sink_id);
  }
  MOCK_METHOD1(MockCreateCompositorFrameSink,
               void(const FrameSinkId& frame_sink_id));
  MOCK_METHOD2(RegisterFrameSinkHierarchy,
               void(const FrameSinkId& parent, const FrameSinkId& child));
  MOCK_METHOD2(UnregisterFrameSinkHierarchy,
               void(const FrameSinkId& parent, const FrameSinkId& child));
  MOCK_METHOD2(AssignTemporaryReference,
               void(const SurfaceId& surface_id, const FrameSinkId& owner));
  MOCK_METHOD1(DropTemporaryReference, void(const SurfaceId& surface_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFrameSinkManagerImpl);
};

}  // namespace

class HostFrameSinkManagerTest : public testing::Test {
 public:
  HostFrameSinkManagerTest() = default;
  ~HostFrameSinkManagerTest() override = default;

  HostFrameSinkManager& host() { return *host_manager_; }

  MockFrameSinkManagerImpl& impl() { return *manager_impl_; }

  std::unique_ptr<CompositorFrameSinkSupport> CreateCompositorFrameSinkSupport(
      const FrameSinkId& frame_sink_id,
      bool is_root) {
    return host_manager_->CreateCompositorFrameSinkSupport(
        nullptr /* client */, frame_sink_id, is_root,
        false /* needs_sync_points */);
  }

  mojom::FrameSinkManagerClient* GetFrameSinkManagerClient() {
    return static_cast<mojom::FrameSinkManagerClient*>(host_manager_.get());
  }

  bool FrameSinkDataExists(const FrameSinkId& frame_sink_id) {
    return host_manager_->frame_sink_data_map_.count(frame_sink_id) > 0;
  }

  // testing::Test:
  void SetUp() override {
    manager_impl_ =
        base::MakeUnique<testing::NiceMock<MockFrameSinkManagerImpl>>();
    host_manager_ = base::MakeUnique<HostFrameSinkManager>();

    manager_impl_->SetLocalClient(host_manager_.get());
    host_manager_->SetLocalManager(manager_impl_.get());
  }

  void TearDown() override {
    host_manager_.reset();
    manager_impl_.reset();
  }

 private:
  std::unique_ptr<HostFrameSinkManager> host_manager_;
  std::unique_ptr<testing::NiceMock<MockFrameSinkManagerImpl>> manager_impl_;

  DISALLOW_COPY_AND_ASSIGN(HostFrameSinkManagerTest);
};

// Verify that creating and destroying a CompositorFrameSink using
// mojom::CompositorFrameSink works correctly.
TEST_F(HostFrameSinkManagerTest, CreateMojomCompositorFrameSink) {
  // Calling CreateCompositorFrameSink() should first register the frame sink
  // and then request to create it.
  EXPECT_CALL(impl(), RegisterFrameSinkId(kFrameSinkChild1));

  FakeHostFrameSinkClient client;
  host().RegisterFrameSinkId(kFrameSinkChild1, &client);

  EXPECT_CALL(impl(), MockCreateCompositorFrameSink(kFrameSinkChild1));
  host().CreateCompositorFrameSink(kFrameSinkChild1, nullptr /* request */,
                                   nullptr /* client */);

  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  // Register should call through to FrameSinkManagerImpl and should work even
  // though |kFrameSinkParent1| was not created yet.
  EXPECT_CALL(impl(),
              RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1));
  host().RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);

  // Destroying the CompositorFrameSink should invalidate it in viz.
  EXPECT_CALL(impl(), InvalidateFrameSinkId(kFrameSinkChild1));

  // We should still have the hierarchy data for |kFrameSinkChild1|.
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  // Unregister should work after the CompositorFrameSink is destroyed.
  EXPECT_CALL(impl(), UnregisterFrameSinkHierarchy(kFrameSinkParent1,
                                                   kFrameSinkChild1));
  host().UnregisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);
  host().InvalidateFrameSinkId(kFrameSinkChild1);

  // Data for |kFrameSinkChild1| should be deleted now.
  EXPECT_FALSE(FrameSinkDataExists(kFrameSinkChild1));
}

// Verify that that creating two CompositorFrameSinkSupports works.
TEST_F(HostFrameSinkManagerTest, CreateCompositorFrameSinkSupport) {
  auto support_client =
      CreateCompositorFrameSinkSupport(kFrameSinkChild1, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  auto support_parent =
      CreateCompositorFrameSinkSupport(kFrameSinkParent1, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkParent1));

  // Register should call through to FrameSinkManagerImpl.
  EXPECT_CALL(impl(),
              RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1));
  host().RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);

  EXPECT_CALL(impl(), UnregisterFrameSinkHierarchy(kFrameSinkParent1,
                                                   kFrameSinkChild1));
  host().UnregisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);

  // We should still have the CompositorFrameSink data for |kFrameSinkChild1|.
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  support_client.reset();

  // Data for |kFrameSinkChild1| should be deleted now.
  EXPECT_FALSE(FrameSinkDataExists(kFrameSinkChild1));

  support_parent.reset();

  // Data for |kFrameSinkParent1| should be deleted now.
  EXPECT_FALSE(FrameSinkDataExists(kFrameSinkParent1));
}

TEST_F(HostFrameSinkManagerTest, AssignTemporaryReference) {
  FakeHostFrameSinkClient client;
  host().RegisterFrameSinkId(kFrameSinkParent1, &client);
  host().RegisterFrameSinkId(kFrameSinkChild1, &client);

  const SurfaceId surface_id = MakeSurfaceId(kFrameSinkChild1, 1);
  auto support = CreateCompositorFrameSinkSupport(surface_id.frame_sink_id(),
                                                  false /* is_root */);

  host().RegisterFrameSinkHierarchy(kFrameSinkParent1,
                                    surface_id.frame_sink_id());

  // When HostFrameSinkManager gets OnFirstSurfaceActivation() it should assign
  // the temporary reference to the registered parent |kFrameSinkParent1|.
  EXPECT_CALL(impl(), AssignTemporaryReference(surface_id, kFrameSinkParent1));
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(surface_id));
}

TEST_F(HostFrameSinkManagerTest, DropTemporaryReference) {
  const SurfaceId surface_id = MakeSurfaceId(kFrameSinkChild1, 1);
  auto support = CreateCompositorFrameSinkSupport(surface_id.frame_sink_id(),
                                                  false /* is_root */);

  // When HostFrameSinkManager gets OnFirstSurfaceActivation() it should find no
  // registered parent and drop the temporary reference.
  EXPECT_CALL(impl(), DropTemporaryReference(surface_id));
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(surface_id));
}

// Verify that we drop the temporary reference to a new surface if the frame
// sink that corresponds to the new surface has been invalidated.
TEST_F(HostFrameSinkManagerTest, DropTemporaryReferenceForStaleClient) {
  FakeHostFrameSinkClient client;
  host().RegisterFrameSinkId(kFrameSinkChild1, &client);
  auto support_client =
      CreateCompositorFrameSinkSupport(kFrameSinkChild1, false /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  host().RegisterFrameSinkId(kFrameSinkParent1, &client);
  auto support_parent =
      CreateCompositorFrameSinkSupport(kFrameSinkParent1, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkParent1));

  // Register should call through to FrameSinkManagerImpl.
  EXPECT_CALL(impl(),
              RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1));
  host().RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);

  const SurfaceId client_surface_id = MakeSurfaceId(kFrameSinkChild1, 1);
  EXPECT_CALL(impl(), DropTemporaryReference(client_surface_id)).Times(0);
  EXPECT_CALL(impl(),
              AssignTemporaryReference(client_surface_id, kFrameSinkParent1))
      .Times(1);
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(client_surface_id));
  testing::Mock::VerifyAndClearExpectations(&impl());

  // Invaidating the client should cause the next SurfaceId to be dropped.
  support_client.reset();
  host().InvalidateFrameSinkId(kFrameSinkChild1);

  const SurfaceId client_surface_id2 = MakeSurfaceId(kFrameSinkChild1, 2);
  EXPECT_CALL(impl(), DropTemporaryReference(client_surface_id2)).Times(1);
  EXPECT_CALL(impl(), AssignTemporaryReference(client_surface_id2, _)).Times(0);
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(client_surface_id2));

  support_parent.reset();
  host().InvalidateFrameSinkId(kFrameSinkParent1);
}

// Verify that multiple parents in the frame sink hierarchy works.
TEST_F(HostFrameSinkManagerTest, HierarchyMultipleParents) {
  FakeHostFrameSinkClient client;

  // Register two parent and child CompositorFrameSink.
  const FrameSinkId& id_parent1 = kFrameSinkParent1;
  host().RegisterFrameSinkId(id_parent1, &client);
  auto support_parent1 =
      CreateCompositorFrameSinkSupport(id_parent1, true /* is_root */);

  const FrameSinkId& id_parent2 = kFrameSinkChild1;
  host().RegisterFrameSinkId(id_parent2, &client);
  auto support_parent2 =
      CreateCompositorFrameSinkSupport(id_parent2, true /* is_root */);

  const FrameSinkId& id_child = kFrameSinkParent2;
  host().RegisterFrameSinkId(id_child, &client);
  auto support_child =
      CreateCompositorFrameSinkSupport(id_child, false /* is_root */);

  // Register |id_parent1| in hierarchy first, this is the original window
  // embedding.
  EXPECT_CALL(impl(), RegisterFrameSinkHierarchy(id_parent1, id_child));
  host().RegisterFrameSinkHierarchy(id_parent1, id_child);

  // Register |id_parent2| in hierarchy second, this is a second embedding for
  // something like alt-tab on a different monitor.
  EXPECT_CALL(impl(), RegisterFrameSinkHierarchy(id_parent2, id_child));
  host().RegisterFrameSinkHierarchy(id_parent2, id_child);
  testing::Mock::VerifyAndClearExpectations(&impl());

  // The oldest registered parent in the hierarchy is assigned the temporary
  // reference.
  const SurfaceId surface_id = MakeSurfaceId(id_child, 1);
  EXPECT_CALL(impl(), AssignTemporaryReference(surface_id, id_parent1));
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(surface_id));
  testing::Mock::VerifyAndClearExpectations(&impl());

  // Unregistering hierarchy with multiple parents should also work.
  EXPECT_CALL(impl(), UnregisterFrameSinkHierarchy(id_parent2, id_child));
  host().UnregisterFrameSinkHierarchy(id_parent2, id_child);

  EXPECT_CALL(impl(), UnregisterFrameSinkHierarchy(id_parent1, id_child));
  host().UnregisterFrameSinkHierarchy(id_parent1, id_child);
}

// Verify that we drop the temporary reference to a new surface if the only
// frame sink registered as an embedder has been invalidated.
TEST_F(HostFrameSinkManagerTest, DropTemporaryReferenceForInvalidatedParent) {
  FakeHostFrameSinkClient client;
  host().RegisterFrameSinkId(kFrameSinkChild1, &client);
  auto support_client =
      CreateCompositorFrameSinkSupport(kFrameSinkChild1, false /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  host().RegisterFrameSinkId(kFrameSinkParent1, &client);
  auto support_parent =
      CreateCompositorFrameSinkSupport(kFrameSinkParent1, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkParent1));

  // Register should call through to FrameSinkManagerImpl.
  EXPECT_CALL(impl(),
              RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1));
  host().RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);

  const SurfaceId client_surface_id = MakeSurfaceId(kFrameSinkChild1, 1);
  EXPECT_CALL(impl(), DropTemporaryReference(client_surface_id)).Times(0);
  EXPECT_CALL(impl(),
              AssignTemporaryReference(client_surface_id, kFrameSinkParent1))
      .Times(1);
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(client_surface_id));
  testing::Mock::VerifyAndClearExpectations(&impl());

  // Invaidating the parent should cause the next SurfaceId to be dropped
  // because there is no registered frame sink as the parent.
  support_parent.reset();
  host().InvalidateFrameSinkId(kFrameSinkParent1);

  const SurfaceId client_surface_id2 = MakeSurfaceId(kFrameSinkChild1, 2);
  EXPECT_CALL(impl(), DropTemporaryReference(client_surface_id2)).Times(1);
  EXPECT_CALL(impl(), AssignTemporaryReference(client_surface_id2, _)).Times(0);
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(client_surface_id2));

  support_client.reset();
  host().InvalidateFrameSinkId(kFrameSinkChild1);
}

TEST_F(HostFrameSinkManagerTest, DisplayRootTemporaryReference) {
  const SurfaceId surface_id = MakeSurfaceId(kFrameSinkParent1, 1);
  auto support = CreateCompositorFrameSinkSupport(surface_id.frame_sink_id(),
                                                  true /* is_root */);

  // When HostFrameSinkManager gets OnFirstSurfaceActivation() it should do
  // nothing since |kFrameSinkParent1| is a display root.
  EXPECT_CALL(impl(), DropTemporaryReference(surface_id)).Times(0);
  EXPECT_CALL(impl(), AssignTemporaryReference(surface_id, _)).Times(0);
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(surface_id));
}

}  // namespace test
}  // namespace viz
