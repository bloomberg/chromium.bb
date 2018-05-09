// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_frame_sink_manager.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_host_frame_sink_client.h"
#include "components/viz/test/fake_surface_observer.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "components/viz/test/mock_display_client.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace viz {
namespace {

constexpr FrameSinkId kFrameSinkParent1(1, 1);
constexpr FrameSinkId kFrameSinkParent2(2, 1);
constexpr FrameSinkId kFrameSinkChild1(3, 1);

ACTION_P(InvokeClosure, closure) {
  closure.Run();
}

// Makes a SurfaceId with a default nonce.
SurfaceId MakeSurfaceId(const FrameSinkId& frame_sink_id, uint32_t parent_id) {
  return SurfaceId(
      frame_sink_id,
      LocalSurfaceId(parent_id, base::UnguessableToken::Deserialize(0, 1u)));
}

// Holds the four interface objects needed to create a RootCompositorFrameSink.
struct RootCompositorFrameSinkData {
  mojom::RootCompositorFrameSinkParamsPtr BuildParams(
      const FrameSinkId& frame_sink_id) {
    auto params = mojom::RootCompositorFrameSinkParams::New();
    params->frame_sink_id = frame_sink_id;
    params->widget = gpu::kNullSurfaceHandle;
    params->compositor_frame_sink = MakeRequest(&compositor_frame_sink);
    params->compositor_frame_sink_client =
        compositor_frame_sink_client.BindInterfacePtr().PassInterface();
    params->display_private = MakeRequest(&display_private);
    params->display_client = display_client.BindInterfacePtr().PassInterface();
    return params;
  }

  mojom::CompositorFrameSinkAssociatedPtr compositor_frame_sink;
  MockCompositorFrameSinkClient compositor_frame_sink_client;
  mojom::DisplayPrivateAssociatedPtr display_private;
  MockDisplayClient display_client;
};

// A mock implementation of mojom::FrameSinkManager.
class MockFrameSinkManagerImpl : public FrameSinkManagerImpl {
 public:
  MockFrameSinkManagerImpl() = default;
  ~MockFrameSinkManagerImpl() override = default;

  // mojom::FrameSinkManager:
  MOCK_METHOD1(RegisterFrameSinkId, void(const FrameSinkId& frame_sink_id));
  MOCK_METHOD1(InvalidateFrameSinkId, void(const FrameSinkId& frame_sink_id));
  MOCK_METHOD2(SetFrameSinkDebugLabel,
               void(const FrameSinkId& frame_sink_id,
                    const std::string& debug_label));
  // Work around for gmock not supporting move-only types.
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      mojom::CompositorFrameSinkRequest request,
      mojom::CompositorFrameSinkClientPtr client) override {
    MockCreateCompositorFrameSink(frame_sink_id);
  }
  MOCK_METHOD1(MockCreateCompositorFrameSink,
               void(const FrameSinkId& frame_sink_id));
  void CreateRootCompositorFrameSink(
      mojom::RootCompositorFrameSinkParamsPtr params) override {
    MockCreateRootCompositorFrameSink(params->frame_sink_id);
  }
  MOCK_METHOD1(MockCreateRootCompositorFrameSink,
               void(const FrameSinkId& frame_sink_id));
  void DestroyCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      DestroyCompositorFrameSinkCallback callback) override {
    MockDestroyCompositorFrameSink(frame_sink_id);
    std::move(callback).Run();
  }
  MOCK_METHOD1(MockDestroyCompositorFrameSink,
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

class HostFrameSinkManagerTestBase : public testing::Test {
 public:
  HostFrameSinkManagerTestBase() = default;
  ~HostFrameSinkManagerTestBase() override = default;

  HostFrameSinkManager& host() { return *host_manager_; }

  MockFrameSinkManagerImpl& impl() { return *manager_impl_; }

  mojom::FrameSinkManagerClient* GetFrameSinkManagerClient() {
    return static_cast<mojom::FrameSinkManagerClient*>(host_manager_.get());
  }

  bool FrameSinkDataExists(const FrameSinkId& frame_sink_id) const {
    return host_manager_->frame_sink_data_map_.count(frame_sink_id) > 0;
  }

  bool IsBoundToFrameSinkManager() {
    return host_manager_->frame_sink_manager_ptr_.is_bound() ||
           host_manager_->binding_.is_bound();
  }

  bool DisplayHitTestQueryExists(const FrameSinkId& frame_sink_id) {
    return host_manager_->display_hit_test_query_.count(frame_sink_id) > 0;
  }

  // testing::Test:
  void TearDown() override {
    host_manager_.reset();
    manager_impl_.reset();
  }

 protected:
  std::unique_ptr<HostFrameSinkManager> host_manager_;
  std::unique_ptr<testing::NiceMock<MockFrameSinkManagerImpl>> manager_impl_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostFrameSinkManagerTestBase);
};

class HostFrameSinkManagerLocalTest : public HostFrameSinkManagerTestBase {
 public:
  HostFrameSinkManagerLocalTest() = default;
  ~HostFrameSinkManagerLocalTest() override = default;

  std::unique_ptr<CompositorFrameSinkSupport> CreateCompositorFrameSinkSupport(
      const FrameSinkId& frame_sink_id,
      bool is_root) {
    return host_manager_->CreateCompositorFrameSinkSupport(
        nullptr /* client */, frame_sink_id, is_root,
        false /* needs_sync_points */);
  }

  // testing::Test:
  void SetUp() override {
    manager_impl_ =
        std::make_unique<testing::NiceMock<MockFrameSinkManagerImpl>>();
    host_manager_ = std::make_unique<HostFrameSinkManager>();

    manager_impl_->SetLocalClient(host_manager_.get());
    host_manager_->SetLocalManager(manager_impl_.get());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HostFrameSinkManagerLocalTest);
};

class HostFrameSinkManagerRemoteTest : public HostFrameSinkManagerTestBase {
 public:
  HostFrameSinkManagerRemoteTest() = default;
  ~HostFrameSinkManagerRemoteTest() override = default;

  // Destroys FrameSinkManagerImpl which kills the message pipes.
  void KillGpu() { manager_impl_.reset(); }

  // Connects HostFrameSinkManager to FrameSinkManagerImpl using Mojo.
  void ConnectToGpu() {
    DCHECK(!manager_impl_);

    manager_impl_ =
        std::make_unique<testing::NiceMock<MockFrameSinkManagerImpl>>();

    mojom::FrameSinkManagerPtr frame_sink_manager;
    mojom::FrameSinkManagerRequest frame_sink_manager_request =
        mojo::MakeRequest(&frame_sink_manager);
    mojom::FrameSinkManagerClientPtr frame_sink_manager_client;
    mojom::FrameSinkManagerClientRequest frame_sink_manager_client_request =
        mojo::MakeRequest(&frame_sink_manager_client);

    host_manager_->BindAndSetManager(
        std::move(frame_sink_manager_client_request), nullptr,
        std::move(frame_sink_manager));
    manager_impl_->BindAndSetClient(std::move(frame_sink_manager_request),
                                    nullptr,
                                    std::move(frame_sink_manager_client));
  }

  // testing::Test:
  void SetUp() override {
    host_manager_ = std::make_unique<HostFrameSinkManager>();
    ConnectToGpu();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HostFrameSinkManagerRemoteTest);
};

// Verify that creating and destroying a CompositorFrameSink using
// mojom::CompositorFrameSink works correctly.
TEST_F(HostFrameSinkManagerLocalTest, CreateMojomCompositorFrameSink) {
  FakeHostFrameSinkClient host_client;

  // Register then create CompositorFrameSink for child.
  EXPECT_CALL(impl(), RegisterFrameSinkId(kFrameSinkChild1));
  host().RegisterFrameSinkId(kFrameSinkChild1, &host_client);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  EXPECT_CALL(impl(), MockCreateCompositorFrameSink(kFrameSinkChild1));
  host().CreateCompositorFrameSink(kFrameSinkChild1, nullptr /* request */,
                                   nullptr /* client */);
  testing::Mock::VerifyAndClearExpectations(&impl());

  // Register but don't actually create CompositorFrameSink for parent.
  host().RegisterFrameSinkId(kFrameSinkParent1, &host_client);

  // Register should call through to FrameSinkManagerImpl and should work even
  // though |kFrameSinkParent1| was not created yet.
  EXPECT_CALL(impl(),
              RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1));
  host().RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);

  // Destroy the CompositorFrameSink.
  EXPECT_CALL(impl(), InvalidateFrameSinkId(kFrameSinkChild1));
  host().InvalidateFrameSinkId(kFrameSinkChild1);
  testing::Mock::VerifyAndClearExpectations(&impl());

  // Unregister should work after the CompositorFrameSink is destroyed.
  EXPECT_CALL(impl(), UnregisterFrameSinkHierarchy(kFrameSinkParent1,
                                                   kFrameSinkChild1));
  host().UnregisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);

  // Data for |kFrameSinkChild1| should be deleted now.
  EXPECT_FALSE(FrameSinkDataExists(kFrameSinkChild1));
}

// Verify that frame_token is sent HostFrameSinkClient if and only if the frame
// is active.
TEST_F(HostFrameSinkManagerLocalTest, CommunicateFrameToken) {
  FakeHostFrameSinkClient host_client_parent;
  FakeHostFrameSinkClient host_client_child;
  uint32_t frame_token1 = 10u;
  FrameSinkId kParentFrameSink(3, 0);
  FrameSinkId kChildFrameSink1(65563, 0);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  host().RegisterFrameSinkId(kParentFrameSink, &host_client_parent);
  host().RegisterFrameSinkId(kChildFrameSink1, &host_client_child);
  auto support =
      CreateCompositorFrameSinkSupport(kParentFrameSink, true /* is_root */);

  CompositorFrame compositor_frame = CompositorFrameBuilder()
                                         .AddDefaultRenderPass()
                                         .SetFrameToken(frame_token1)
                                         .SetActivationDependencies({child_id1})
                                         .Build();
  support->SubmitCompositorFrame(parent_id1.local_surface_id(),
                                 std::move(compositor_frame));

  Surface* parent_surface = support->GetLastCreatedSurfaceForTesting();
  EXPECT_TRUE(parent_surface->has_deadline());
  EXPECT_FALSE(parent_surface->HasActiveFrame());
  EXPECT_TRUE(parent_surface->HasPendingFrame());
  // Since the frame is not activated, |frame_token| is not sent to
  // HostFrameSinkClient.
  EXPECT_EQ(0u, host_client_parent.last_frame_token_seen());

  parent_surface->ActivatePendingFrameForDeadline(base::nullopt);
  EXPECT_FALSE(parent_surface->has_deadline());
  EXPECT_TRUE(parent_surface->HasActiveFrame());
  EXPECT_FALSE(parent_surface->HasPendingFrame());
  // Since the frame is now activated, |frame_token| is sent to
  // HostFrameSinkClient.
  EXPECT_EQ(10u, host_client_parent.last_frame_token_seen());
}

// Verify that that creating two CompositorFrameSinkSupports works.
TEST_F(HostFrameSinkManagerLocalTest, CreateCompositorFrameSinkSupport) {
  FakeHostFrameSinkClient host_client;

  host().RegisterFrameSinkId(kFrameSinkChild1, &host_client);
  auto support_child =
      CreateCompositorFrameSinkSupport(kFrameSinkChild1, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  host().RegisterFrameSinkId(kFrameSinkParent1, &host_client);
  auto support_parent =
      CreateCompositorFrameSinkSupport(kFrameSinkParent1, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkParent1));

  // Verify that registering and unregistering frame sink hierarchy works.
  EXPECT_CALL(impl(),
              RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1));
  host().RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);
  testing::Mock::VerifyAndClearExpectations(&impl());

  EXPECT_CALL(impl(), UnregisterFrameSinkHierarchy(kFrameSinkParent1,
                                                   kFrameSinkChild1));
  host().UnregisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);

  // We should still have the CompositorFrameSink data for |kFrameSinkChild1|.
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  // Data for |kFrameSinkChild1| should be deleted when everything is destroyed.
  support_child.reset();
  host().InvalidateFrameSinkId(kFrameSinkChild1);
  EXPECT_FALSE(FrameSinkDataExists(kFrameSinkChild1));

  // Data for |kFrameSinkParent1| should be deleted when everything is
  // destroyed.
  support_parent.reset();
  host().InvalidateFrameSinkId(kFrameSinkParent1);
  EXPECT_FALSE(FrameSinkDataExists(kFrameSinkParent1));
}

TEST_F(HostFrameSinkManagerLocalTest, AssignTemporaryReference) {
  FakeHostFrameSinkClient host_client;
  host().RegisterFrameSinkId(kFrameSinkParent1, &host_client);

  const SurfaceId surface_id = MakeSurfaceId(kFrameSinkChild1, 1);
  host().RegisterFrameSinkId(surface_id.frame_sink_id(), &host_client);
  auto support = CreateCompositorFrameSinkSupport(surface_id.frame_sink_id(),
                                                  false /* is_root */);

  host().RegisterFrameSinkHierarchy(kFrameSinkParent1,
                                    surface_id.frame_sink_id());

  // When HostFrameSinkManager gets OnSurfaceCreated() it should assign
  // the temporary reference to the registered parent |kFrameSinkParent1|.
  EXPECT_CALL(impl(), AssignTemporaryReference(surface_id, kFrameSinkParent1));
  GetFrameSinkManagerClient()->OnSurfaceCreated(surface_id);
}

// Verify that we drop temporary reference to a surface that doesn't have any
// registered parent.
TEST_F(HostFrameSinkManagerLocalTest, DropTemporaryReference) {
  FakeHostFrameSinkClient host_client;

  const SurfaceId surface_id = MakeSurfaceId(kFrameSinkChild1, 1);
  host().RegisterFrameSinkId(surface_id.frame_sink_id(), &host_client);
  auto support = CreateCompositorFrameSinkSupport(surface_id.frame_sink_id(),
                                                  false /* is_root */);

  // When HostFrameSinkManager gets OnSurfaceCreated() it should find no
  // registered parent and drop the temporary reference.
  EXPECT_CALL(impl(), DropTemporaryReference(surface_id));
  GetFrameSinkManagerClient()->OnSurfaceCreated(surface_id);
}

// Verify that we drop the temporary reference to a new surface if the frame
// sink that corresponds to the new surface has been invalidated.
TEST_F(HostFrameSinkManagerLocalTest, DropTemporaryReferenceForStaleClient) {
  FakeHostFrameSinkClient host_client;

  host().RegisterFrameSinkId(kFrameSinkChild1, &host_client);
  auto support_child =
      CreateCompositorFrameSinkSupport(kFrameSinkChild1, false /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  host().RegisterFrameSinkId(kFrameSinkParent1, &host_client);
  auto support_parent =
      CreateCompositorFrameSinkSupport(kFrameSinkParent1, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkParent1));

  // Register should call through to FrameSinkManagerImpl.
  EXPECT_CALL(impl(),
              RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1));
  host().RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);

  // Verify that temporary reference is assigned correctly before invalidation.
  const SurfaceId client_surface_id = MakeSurfaceId(kFrameSinkChild1, 1);
  EXPECT_CALL(impl(), DropTemporaryReference(client_surface_id)).Times(0);
  EXPECT_CALL(impl(),
              AssignTemporaryReference(client_surface_id, kFrameSinkParent1))
      .Times(1);
  GetFrameSinkManagerClient()->OnSurfaceCreated(client_surface_id);
  testing::Mock::VerifyAndClearExpectations(&impl());

  // Invaidating the child should cause the temporary reference to the next
  // SurfaceId to be dropped.
  support_child.reset();
  host().InvalidateFrameSinkId(kFrameSinkChild1);

  const SurfaceId client_surface_id2 = MakeSurfaceId(kFrameSinkChild1, 2);
  EXPECT_CALL(impl(), DropTemporaryReference(client_surface_id2)).Times(1);
  EXPECT_CALL(impl(), AssignTemporaryReference(client_surface_id2, _)).Times(0);
  GetFrameSinkManagerClient()->OnSurfaceCreated(client_surface_id2);

  support_parent.reset();
  host().InvalidateFrameSinkId(kFrameSinkParent1);
}

// Verify that multiple parents in the frame sink hierarchy works.
TEST_F(HostFrameSinkManagerLocalTest, HierarchyMultipleParents) {
  FakeHostFrameSinkClient host_client;

  // Register two parent and child CompositorFrameSink.
  const FrameSinkId& id_parent1 = kFrameSinkParent1;
  host().RegisterFrameSinkId(id_parent1, &host_client);
  auto support_parent1 =
      CreateCompositorFrameSinkSupport(id_parent1, true /* is_root */);

  const FrameSinkId& id_parent2 = kFrameSinkChild1;
  host().RegisterFrameSinkId(id_parent2, &host_client);
  auto support_parent2 =
      CreateCompositorFrameSinkSupport(id_parent2, true /* is_root */);

  const FrameSinkId& id_child = kFrameSinkParent2;
  host().RegisterFrameSinkId(id_child, &host_client);
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
  GetFrameSinkManagerClient()->OnSurfaceCreated(surface_id);
  testing::Mock::VerifyAndClearExpectations(&impl());

  // Unregistering hierarchy with multiple parents should also work.
  EXPECT_CALL(impl(), UnregisterFrameSinkHierarchy(id_parent2, id_child));
  host().UnregisterFrameSinkHierarchy(id_parent2, id_child);

  EXPECT_CALL(impl(), UnregisterFrameSinkHierarchy(id_parent1, id_child));
  host().UnregisterFrameSinkHierarchy(id_parent1, id_child);
}

// Verify that we drop the temporary reference to a new surface if the only
// frame sink registered as an embedder has been invalidated.
TEST_F(HostFrameSinkManagerLocalTest,
       DropTemporaryReferenceForInvalidatedParent) {
  FakeHostFrameSinkClient host_client;

  host().RegisterFrameSinkId(kFrameSinkChild1, &host_client);
  auto support_child =
      CreateCompositorFrameSinkSupport(kFrameSinkChild1, false /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  host().RegisterFrameSinkId(kFrameSinkParent1, &host_client);
  auto support_parent =
      CreateCompositorFrameSinkSupport(kFrameSinkParent1, true /* is_root */);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkParent1));

  // Register should call through to FrameSinkManagerImpl.
  EXPECT_CALL(impl(),
              RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1));
  host().RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);

  // Verify that temporary reference is assigned correctly before invalidation.
  const SurfaceId client_surface_id = MakeSurfaceId(kFrameSinkChild1, 1);
  EXPECT_CALL(impl(), DropTemporaryReference(client_surface_id)).Times(0);
  EXPECT_CALL(impl(),
              AssignTemporaryReference(client_surface_id, kFrameSinkParent1))
      .Times(1);
  GetFrameSinkManagerClient()->OnSurfaceCreated(client_surface_id);
  testing::Mock::VerifyAndClearExpectations(&impl());

  // Invaidating the parent should cause the next SurfaceId to be dropped
  // because there is no registered frame sink as the parent.
  support_parent.reset();
  host().InvalidateFrameSinkId(kFrameSinkParent1);

  const SurfaceId client_surface_id2 = MakeSurfaceId(kFrameSinkChild1, 2);
  EXPECT_CALL(impl(), DropTemporaryReference(client_surface_id2)).Times(1);
  EXPECT_CALL(impl(), AssignTemporaryReference(client_surface_id2, _)).Times(0);
  GetFrameSinkManagerClient()->OnSurfaceCreated(client_surface_id2);

  support_child.reset();
  host().InvalidateFrameSinkId(kFrameSinkChild1);
}

TEST_F(HostFrameSinkManagerLocalTest, DisplayRootTemporaryReference) {
  FakeHostFrameSinkClient host_client;

  const SurfaceId surface_id = MakeSurfaceId(kFrameSinkParent1, 1);
  host().RegisterFrameSinkId(surface_id.frame_sink_id(), &host_client);
  auto support = CreateCompositorFrameSinkSupport(surface_id.frame_sink_id(),
                                                  true /* is_root */);

  // When HostFrameSinkManager gets OnSurfaceCreated() it should do
  // nothing since |kFrameSinkParent1| is a display root.
  EXPECT_CALL(impl(), DropTemporaryReference(surface_id)).Times(0);
  EXPECT_CALL(impl(), AssignTemporaryReference(surface_id, _)).Times(0);
  GetFrameSinkManagerClient()->OnSurfaceCreated(surface_id);
}

// Test the creation and desctruction of HitTestAggregator and HitTestQuery.
TEST_F(HostFrameSinkManagerLocalTest, HitTestAggregatorQuery) {
  FakeHostFrameSinkClient client;
  EXPECT_FALSE(FrameSinkDataExists(kFrameSinkChild1));
  host().RegisterFrameSinkId(kFrameSinkChild1, &client);
  EXPECT_TRUE(FrameSinkDataExists(kFrameSinkChild1));

  EXPECT_FALSE(DisplayHitTestQueryExists(kFrameSinkChild1));
  auto support =
      CreateCompositorFrameSinkSupport(kFrameSinkChild1, true /* is_root */);
  support->SetUpHitTest(nullptr /* local_surface_id_lookup_delegate */);
  EXPECT_TRUE(DisplayHitTestQueryExists(kFrameSinkChild1));
  EXPECT_TRUE(support->GetHitTestAggregator());

  host().InvalidateFrameSinkId(kFrameSinkChild1);
  support.reset();
  EXPECT_FALSE(FrameSinkDataExists(kFrameSinkChild1));
  EXPECT_FALSE(DisplayHitTestQueryExists(kFrameSinkChild1));
}

// Verify that HostFrameSinkManager can handle restarting after a GPU crash.
TEST_F(HostFrameSinkManagerRemoteTest, RestartOnGpuCrash) {
  FakeHostFrameSinkClient host_client;

  // Register two FrameSinkIds, hierarchy between them and create a
  // CompositorFrameSink for one.
  host().RegisterFrameSinkId(kFrameSinkParent1, &host_client);
  host().RegisterFrameSinkId(kFrameSinkChild1, &host_client);
  host().RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1);

  RootCompositorFrameSinkData root_data;
  host().CreateRootCompositorFrameSink(
      root_data.BuildParams(kFrameSinkParent1));

  MockCompositorFrameSinkClient compositor_frame_sink_client;
  mojom::CompositorFrameSinkPtr compositor_frame_sink;
  host().CreateCompositorFrameSink(
      kFrameSinkChild1, MakeRequest(&compositor_frame_sink),
      compositor_frame_sink_client.BindInterfacePtr());

  EXPECT_TRUE(IsBoundToFrameSinkManager());

  // Verify registration and CompositorFrameSink creation happened.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(impl(), RegisterFrameSinkId(kFrameSinkParent1));
    EXPECT_CALL(impl(), RegisterFrameSinkId(kFrameSinkChild1));
    EXPECT_CALL(impl(), RegisterFrameSinkHierarchy(kFrameSinkParent1,
                                                   kFrameSinkChild1));
    EXPECT_CALL(impl(), MockCreateRootCompositorFrameSink(kFrameSinkParent1));
    EXPECT_CALL(impl(), MockCreateCompositorFrameSink(kFrameSinkChild1))
        .WillOnce(InvokeClosure(run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Kill the GPU. HostFrameSinkManager will have a connection error on the
  // message pipe and should clear state appropriately.
  KillGpu();
  {
    base::RunLoop run_loop;
    host().SetConnectionLostCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  EXPECT_FALSE(IsBoundToFrameSinkManager());

  // Verify that when HostFrameSinkManager is connected to the GPU again it
  // reregisters FrameSinkIds and FrameSink hierarchy.
  ConnectToGpu();
  {
    base::RunLoop run_loop;
    EXPECT_CALL(impl(), RegisterFrameSinkId(kFrameSinkParent1));
    EXPECT_CALL(impl(), RegisterFrameSinkId(kFrameSinkChild1));
    EXPECT_CALL(impl(),
                RegisterFrameSinkHierarchy(kFrameSinkParent1, kFrameSinkChild1))
        .WillOnce(InvokeClosure(run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_TRUE(IsBoundToFrameSinkManager());
}

// Verify that HostFrameSinkManager does early return when trying to send
// hit-test data after HitTestQuery was deleted.
TEST_F(HostFrameSinkManagerRemoteTest, DeletedHitTestQuery) {
  FakeHostFrameSinkClient host_client;

  // Register a FrameSinkId, and create a RootCompositorFrameSink, which should
  // create a HitTestQuery.
  host().RegisterFrameSinkId(kFrameSinkParent1, &host_client);
  RootCompositorFrameSinkData root_data;
  host().CreateRootCompositorFrameSink(
      root_data.BuildParams(kFrameSinkParent1));

  EXPECT_TRUE(DisplayHitTestQueryExists(kFrameSinkParent1));

  // Verify RootCompositorFrameSink was created on other end of message pipe.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(impl(), MockCreateRootCompositorFrameSink(kFrameSinkParent1))
        .WillOnce(InvokeClosure(run_loop.QuitClosure()));
    run_loop.Run();
  }

  GetFrameSinkManagerClient()->OnAggregatedHitTestRegionListUpdated(
      kFrameSinkParent1, {});
  // Continue to send hit-test data to HitTestQuery associated with
  // kFrameSinkChild1.

  host().InvalidateFrameSinkId(kFrameSinkParent1);
  // Invalidating kFrameSinkChild1 would delete the corresponding HitTestQuery,
  // so further msgs to that HitTestQuery should be dropped.
  EXPECT_FALSE(DisplayHitTestQueryExists(kFrameSinkParent1));
  GetFrameSinkManagerClient()->OnAggregatedHitTestRegionListUpdated(
      kFrameSinkParent1, {});
}

// Verify that HostFrameSinkManager assigns temporary references when connected
// to a remote mojom::FrameSinkManager.
TEST_F(HostFrameSinkManagerRemoteTest, AssignTemporaryReference) {
  FakeHostFrameSinkClient host_client;
  host().RegisterFrameSinkId(kFrameSinkParent1, &host_client);

  const SurfaceId surface_id = MakeSurfaceId(kFrameSinkChild1, 1);
  host().RegisterFrameSinkId(surface_id.frame_sink_id(), &host_client);
  MockCompositorFrameSinkClient compositor_frame_sink_client;
  mojom::CompositorFrameSinkPtr compositor_frame_sink;
  host().CreateCompositorFrameSink(
      kFrameSinkChild1, MakeRequest(&compositor_frame_sink),
      compositor_frame_sink_client.BindInterfacePtr());

  host().RegisterFrameSinkHierarchy(kFrameSinkParent1,
                                    surface_id.frame_sink_id());

  // When HostFrameSinkManager gets OnSuraceCreated() it should assign
  // the temporary reference to the registered parent |kFrameSinkParent1|.
  GetFrameSinkManagerClient()->OnSurfaceCreated(surface_id);

  base::RunLoop run_loop;
  EXPECT_CALL(impl(), AssignTemporaryReference(surface_id, kFrameSinkParent1))
      .WillOnce(InvokeClosure(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(HostFrameSinkManagerRemoteTest, DropTemporaryReference) {
  FakeHostFrameSinkClient host_client;

  const SurfaceId surface_id = MakeSurfaceId(kFrameSinkChild1, 1);
  host().RegisterFrameSinkId(surface_id.frame_sink_id(), &host_client);
  MockCompositorFrameSinkClient compositor_frame_sink_client;
  mojom::CompositorFrameSinkPtr compositor_frame_sink;
  host().CreateCompositorFrameSink(
      kFrameSinkChild1, MakeRequest(&compositor_frame_sink),
      compositor_frame_sink_client.BindInterfacePtr());

  // When HostFrameSinkManager gets OnSuraceCreated() it should find that
  // kFrameSinkChild1 isn't embedded by anything and drop the temporary
  // reference.
  GetFrameSinkManagerClient()->OnSurfaceCreated(surface_id);

  base::RunLoop run_loop;
  EXPECT_CALL(impl(), DropTemporaryReference(surface_id))
      .WillOnce(InvokeClosure(run_loop.QuitClosure()));
  run_loop.Run();
}

// Verify that on lost context a RootCompositorFrameSink can be recreated.
TEST_F(HostFrameSinkManagerRemoteTest, ContextLossRecreateRoot) {
  FakeHostFrameSinkClient host_client;

  // Register a FrameSinkId, and create a RootCompositorFrameSink.
  host().RegisterFrameSinkId(kFrameSinkParent1, &host_client);
  RootCompositorFrameSinkData root_data1;
  host().CreateRootCompositorFrameSink(
      root_data1.BuildParams(kFrameSinkParent1));

  // Verify RootCompositorFrameSink was created on other end of message pipe.
  EXPECT_CALL(impl(), MockCreateRootCompositorFrameSink(kFrameSinkParent1));
  root_data1.compositor_frame_sink.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&impl());

  // Create a new RootCompositorFrameSink and try to connect it with the same
  // FrameSinkId. This will happen if the browser GL context is lost.
  RootCompositorFrameSinkData root_data2;
  host().CreateRootCompositorFrameSink(
      root_data2.BuildParams(kFrameSinkParent1));

  // Verify RootCompositorFrameSink is destroyed and then recreated.
  EXPECT_CALL(impl(), MockDestroyCompositorFrameSink(kFrameSinkParent1));
  EXPECT_CALL(impl(), MockCreateRootCompositorFrameSink(kFrameSinkParent1));
  root_data2.compositor_frame_sink.FlushForTesting();
}

// Verify that on lost context a CompositorFrameSink can be recreated.
TEST_F(HostFrameSinkManagerRemoteTest, ContextLossRecreateNonRoot) {
  FakeHostFrameSinkClient host_client;

  // Register a FrameSinkId and create a CompositorFrameSink.
  host().RegisterFrameSinkId(kFrameSinkChild1, &host_client);
  MockCompositorFrameSinkClient compositor_frame_sink_client1;
  mojom::CompositorFrameSinkPtr compositor_frame_sink1;
  host().CreateCompositorFrameSink(
      kFrameSinkChild1, MakeRequest(&compositor_frame_sink1),
      compositor_frame_sink_client1.BindInterfacePtr());

  // Verify CompositorFrameSink was created on other end of message pipe.
  EXPECT_CALL(impl(), MockCreateCompositorFrameSink(kFrameSinkChild1));
  compositor_frame_sink1.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&impl());

  // Create a new CompositorFrameSink and try to connect it with the same
  // FrameSinkId. This will happen if the client GL context is lost.
  MockCompositorFrameSinkClient compositor_frame_sink_client2;
  mojom::CompositorFrameSinkPtr compositor_frame_sink2;
  host().CreateCompositorFrameSink(
      kFrameSinkChild1, MakeRequest(&compositor_frame_sink2),
      compositor_frame_sink_client2.BindInterfacePtr());

  // Verify CompositorFrameSink is destroyed and then recreated.
  EXPECT_CALL(impl(), MockDestroyCompositorFrameSink(kFrameSinkChild1));
  EXPECT_CALL(impl(), MockCreateCompositorFrameSink(kFrameSinkChild1));
  compositor_frame_sink2.FlushForTesting();
}

}  // namespace viz
