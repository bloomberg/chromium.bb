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
#include "services/viz/compositing/privileged/interfaces/frame_sink_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace viz {
namespace test {
namespace {

constexpr FrameSinkId kParentFrameSinkId(1, 1);
constexpr FrameSinkId kClientFrameSinkId(2, 1);

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

  HostFrameSinkManager& host_manager() { return *host_manager_; }

  MockFrameSinkManagerImpl& manager_impl() { return *manager_impl_; }

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
  EXPECT_CALL(manager_impl(), RegisterFrameSinkId(kClientFrameSinkId));

  FakeHostFrameSinkClient client;
  host_manager().RegisterFrameSinkId(kClientFrameSinkId, &client);

  EXPECT_CALL(manager_impl(),
              MockCreateCompositorFrameSink(kClientFrameSinkId));
  host_manager().CreateCompositorFrameSink(
      kClientFrameSinkId, nullptr /* request */, nullptr /* client */);

  EXPECT_TRUE(FrameSinkDataExists(kClientFrameSinkId));

  // Register should call through to FrameSinkManagerImpl and should work even
  // though |kParentFrameSinkId| was not created yet.
  EXPECT_CALL(manager_impl(), RegisterFrameSinkHierarchy(kParentFrameSinkId,
                                                         kClientFrameSinkId));
  host_manager().RegisterFrameSinkHierarchy(kParentFrameSinkId,
                                            kClientFrameSinkId);

  // Destroying the CompositorFrameSink should invalidate it in viz.
  EXPECT_CALL(manager_impl(), InvalidateFrameSinkId(kClientFrameSinkId));

  // We should still have the hierarchy data for |kClientFrameSinkId|.
  EXPECT_TRUE(FrameSinkDataExists(kClientFrameSinkId));

  // Unregister should work after the CompositorFrameSink is destroyed.
  EXPECT_CALL(manager_impl(), UnregisterFrameSinkHierarchy(kParentFrameSinkId,
                                                           kClientFrameSinkId));
  host_manager().UnregisterFrameSinkHierarchy(kParentFrameSinkId,
                                              kClientFrameSinkId);
  host_manager().InvalidateFrameSinkId(kClientFrameSinkId);

  // Data for |kClientFrameSinkId| should be deleted now.
  EXPECT_FALSE(FrameSinkDataExists(kClientFrameSinkId));
}

// Verify that that creating two CompositorFrameSinkSupports works.
TEST_F(HostFrameSinkManagerTest, CreateCompositorFrameSinkSupport) {
  auto support_client =
      CreateCompositorFrameSinkSupport(kClientFrameSinkId, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kClientFrameSinkId));

  auto support_parent =
      CreateCompositorFrameSinkSupport(kParentFrameSinkId, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kParentFrameSinkId));

  // Register should call through to FrameSinkManagerImpl.
  EXPECT_CALL(manager_impl(), RegisterFrameSinkHierarchy(kParentFrameSinkId,
                                                         kClientFrameSinkId));
  host_manager().RegisterFrameSinkHierarchy(kParentFrameSinkId,
                                            kClientFrameSinkId);

  EXPECT_CALL(manager_impl(), UnregisterFrameSinkHierarchy(kParentFrameSinkId,
                                                           kClientFrameSinkId));
  host_manager().UnregisterFrameSinkHierarchy(kParentFrameSinkId,
                                              kClientFrameSinkId);

  // We should still have the CompositorFrameSink data for |kClientFrameSinkId|.
  EXPECT_TRUE(FrameSinkDataExists(kClientFrameSinkId));

  support_client.reset();

  // Data for |kClientFrameSinkId| should be deleted now.
  EXPECT_FALSE(FrameSinkDataExists(kClientFrameSinkId));

  support_parent.reset();

  // Data for |kParentFrameSinkId| should be deleted now.
  EXPECT_FALSE(FrameSinkDataExists(kParentFrameSinkId));
}

TEST_F(HostFrameSinkManagerTest, AssignTemporaryReference) {
  FakeHostFrameSinkClient client;
  host_manager().RegisterFrameSinkId(kParentFrameSinkId, &client);
  host_manager().RegisterFrameSinkId(kClientFrameSinkId, &client);

  const SurfaceId surface_id = MakeSurfaceId(kClientFrameSinkId, 1);
  auto support = CreateCompositorFrameSinkSupport(surface_id.frame_sink_id(),
                                                  false /* is_root */);

  host_manager().RegisterFrameSinkHierarchy(kParentFrameSinkId,
                                            surface_id.frame_sink_id());

  // When HostFrameSinkManager gets OnFirstSurfaceActivation() it should assign
  // the temporary reference to the registered parent |kParentFrameSinkId|.
  EXPECT_CALL(manager_impl(),
              AssignTemporaryReference(surface_id, kParentFrameSinkId));
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(surface_id));
}

TEST_F(HostFrameSinkManagerTest, DropTemporaryReference) {
  const SurfaceId surface_id = MakeSurfaceId(kClientFrameSinkId, 1);
  auto support = CreateCompositorFrameSinkSupport(surface_id.frame_sink_id(),
                                                  false /* is_root */);

  // When HostFrameSinkManager gets OnFirstSurfaceActivation() it should find no
  // registered parent and drop the temporary reference.
  EXPECT_CALL(manager_impl(), DropTemporaryReference(surface_id));
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(surface_id));
}

// Verify that we drop the temporary reference to a new surface if the frame
// sink that corresponds to the new surface has been invalidated.
TEST_F(HostFrameSinkManagerTest, DropTemporaryReferenceForStaleClient) {
  FakeHostFrameSinkClient client;
  host_manager().RegisterFrameSinkId(kClientFrameSinkId, &client);
  auto support_client =
      CreateCompositorFrameSinkSupport(kClientFrameSinkId, false /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kClientFrameSinkId));

  host_manager().RegisterFrameSinkId(kParentFrameSinkId, &client);
  auto support_parent =
      CreateCompositorFrameSinkSupport(kParentFrameSinkId, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kParentFrameSinkId));

  // Register should call through to FrameSinkManagerImpl.
  EXPECT_CALL(manager_impl(), RegisterFrameSinkHierarchy(kParentFrameSinkId,
                                                         kClientFrameSinkId));
  host_manager().RegisterFrameSinkHierarchy(kParentFrameSinkId,
                                            kClientFrameSinkId);

  const SurfaceId client_surface_id = MakeSurfaceId(kClientFrameSinkId, 1);
  EXPECT_CALL(manager_impl(), DropTemporaryReference(client_surface_id))
      .Times(0);
  EXPECT_CALL(manager_impl(),
              AssignTemporaryReference(client_surface_id, kParentFrameSinkId))
      .Times(1);
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(client_surface_id));
  testing::Mock::VerifyAndClearExpectations(&manager_impl());

  // Invaidating the client should cause the next SurfaceId to be dropped.
  support_client.reset();
  host_manager().InvalidateFrameSinkId(kClientFrameSinkId);

  const SurfaceId client_surface_id2 = MakeSurfaceId(kClientFrameSinkId, 2);
  EXPECT_CALL(manager_impl(), DropTemporaryReference(client_surface_id2))
      .Times(1);
  EXPECT_CALL(manager_impl(), AssignTemporaryReference(client_surface_id2, _))
      .Times(0);
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(client_surface_id2));

  support_parent.reset();
  host_manager().InvalidateFrameSinkId(kParentFrameSinkId);
}

// Verify that we drop the temporary reference to a new surface if the only
// frame sink registered as an embedder has been invalidated.
TEST_F(HostFrameSinkManagerTest, DropTemporaryReferenceForInvalidatedParent) {
  FakeHostFrameSinkClient client;
  host_manager().RegisterFrameSinkId(kClientFrameSinkId, &client);
  auto support_client =
      CreateCompositorFrameSinkSupport(kClientFrameSinkId, false /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kClientFrameSinkId));

  host_manager().RegisterFrameSinkId(kParentFrameSinkId, &client);
  auto support_parent =
      CreateCompositorFrameSinkSupport(kParentFrameSinkId, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kParentFrameSinkId));

  // Register should call through to FrameSinkManagerImpl.
  EXPECT_CALL(manager_impl(), RegisterFrameSinkHierarchy(kParentFrameSinkId,
                                                         kClientFrameSinkId));
  host_manager().RegisterFrameSinkHierarchy(kParentFrameSinkId,
                                            kClientFrameSinkId);

  const SurfaceId client_surface_id = MakeSurfaceId(kClientFrameSinkId, 1);
  EXPECT_CALL(manager_impl(), DropTemporaryReference(client_surface_id))
      .Times(0);
  EXPECT_CALL(manager_impl(),
              AssignTemporaryReference(client_surface_id, kParentFrameSinkId))
      .Times(1);
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(client_surface_id));
  testing::Mock::VerifyAndClearExpectations(&manager_impl());

  // Invaidating the parent should cause the next SurfaceId to be dropped
  // because there is no registered frame sink as the parent.
  support_parent.reset();
  host_manager().InvalidateFrameSinkId(kParentFrameSinkId);

  const SurfaceId client_surface_id2 = MakeSurfaceId(kClientFrameSinkId, 2);
  EXPECT_CALL(manager_impl(), DropTemporaryReference(client_surface_id2))
      .Times(1);
  EXPECT_CALL(manager_impl(), AssignTemporaryReference(client_surface_id2, _))
      .Times(0);
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(client_surface_id2));

  support_client.reset();
  host_manager().InvalidateFrameSinkId(kClientFrameSinkId);
}

TEST_F(HostFrameSinkManagerTest, DisplayRootTemporaryReference) {
  const SurfaceId surface_id = MakeSurfaceId(kParentFrameSinkId, 1);
  auto support = CreateCompositorFrameSinkSupport(surface_id.frame_sink_id(),
                                                  true /* is_root */);

  // When HostFrameSinkManager gets OnFirstSurfaceActivation() it should do
  // nothing since |kParentFrameSinkId| is a display root.
  EXPECT_CALL(manager_impl(), DropTemporaryReference(surface_id)).Times(0);
  EXPECT_CALL(manager_impl(), AssignTemporaryReference(surface_id, _)).Times(0);
  GetFrameSinkManagerClient()->OnFirstSurfaceActivation(
      MakeSurfaceInfo(surface_id));
}

}  // namespace test
}  // namespace viz
