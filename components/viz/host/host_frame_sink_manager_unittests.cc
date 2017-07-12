// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_frame_sink_manager.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "components/viz/common/frame_sink_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace test {
namespace {

constexpr FrameSinkId kFrameSinkId1(1, 1);
constexpr FrameSinkId kFrameSinkId2(1, 1);

ACTION_P(InvokeClosure, closure) {
  closure.Run();
}

// A stub CompositorFrameSinkClient that does nothing.
class StubCompositorFrameSinkClient
    : public cc::mojom::CompositorFrameSinkClient {
 public:
  StubCompositorFrameSinkClient() : binding_(this) {}
  ~StubCompositorFrameSinkClient() override = default;

  cc::mojom::CompositorFrameSinkClientPtr GetInterfacePtr() {
    cc::mojom::CompositorFrameSinkClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client));
    return client;
  }

 private:
  // cc::mojom::CompositorFrameSinkClient:
  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override {}
  void OnBeginFrame(const cc::BeginFrameArgs& begin_frame_args) override {}
  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override {}

  mojo::Binding<cc::mojom::CompositorFrameSinkClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(StubCompositorFrameSinkClient);
};

// A mock implementation of mojom::FrameSinkManager.
class MockFrameSinkManagerImpl : public cc::mojom::FrameSinkManager {
 public:
  MockFrameSinkManagerImpl() : binding_(this) {}
  ~MockFrameSinkManagerImpl() override = default;

  void BindAndSetClient(cc::mojom::FrameSinkManagerRequest request,
                        cc::mojom::FrameSinkManagerClientPtr client) {
    binding_.Bind(std::move(request));
    client_ = std::move(client);
  }

  // cc::mojom::FrameSinkManager:
  // TODO(kylechar): See if we can mock functions with InterfacePtrs parameters.
  void CreateRootCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      cc::mojom::CompositorFrameSinkAssociatedRequest request,
      cc::mojom::CompositorFrameSinkPrivateRequest private_request,
      cc::mojom::CompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateAssociatedRequest display_private_request)
      override {}
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      cc::mojom::CompositorFrameSinkRequest request,
      cc::mojom::CompositorFrameSinkPrivateRequest private_request,
      cc::mojom::CompositorFrameSinkClientPtr client) override {}
  MOCK_METHOD2(RegisterFrameSinkHierarchy,
               void(const FrameSinkId& parent, const FrameSinkId& child));
  MOCK_METHOD2(UnregisterFrameSinkHierarchy,
               void(const FrameSinkId& parent, const FrameSinkId& child));
  MOCK_METHOD1(DropTemporaryReference, void(const SurfaceId& surface_id));

 private:
  mojo::Binding<cc::mojom::FrameSinkManager> binding_;
  cc::mojom::FrameSinkManagerClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(MockFrameSinkManagerImpl);
};

}  // namespace

class HostFrameSinkManagerTest : public testing::Test {
 public:
  HostFrameSinkManagerTest() = default;
  ~HostFrameSinkManagerTest() override = default;

  HostFrameSinkManager& host_manager() { return *host_manager_; }

  MockFrameSinkManagerImpl& manager_impl() { return *manager_impl_; }

  // testing::Test:
  void SetUp() override {
    manager_impl_ = base::MakeUnique<MockFrameSinkManagerImpl>();
    host_manager_ = base::MakeUnique<HostFrameSinkManager>();

    // Connect HostFrameSinkManager and FrameSinkManagerImpl.
    cc::mojom::FrameSinkManagerClientPtr host_mojo;
    cc::mojom::FrameSinkManagerClientRequest host_mojo_request =
        mojo::MakeRequest(&host_mojo);
    cc::mojom::FrameSinkManagerPtr manager_mojo;
    cc::mojom::FrameSinkManagerRequest manager_impl_request =
        mojo::MakeRequest(&manager_mojo);
    manager_impl_->BindAndSetClient(std::move(manager_impl_request),
                                    std::move(host_mojo));
    host_manager_->BindAndSetManager(std::move(host_mojo_request),
                                     message_loop_.task_runner(),
                                     std::move(manager_mojo));
  }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<HostFrameSinkManager> host_manager_;
  std::unique_ptr<MockFrameSinkManagerImpl> manager_impl_;

  DISALLOW_COPY_AND_ASSIGN(HostFrameSinkManagerTest);
};

// Verify that when destroying a CompositorFrameSink with registered FrameSink
// hierarchy, the hierarchy is automatically unregistered.
TEST_F(HostFrameSinkManagerTest, UnregisterHierarchyOnDestroy) {
  base::RunLoop run_loop;

  cc::mojom::CompositorFrameSinkPtr frame_sink;
  StubCompositorFrameSinkClient frame_sink_client;
  host_manager().CreateCompositorFrameSink(kFrameSinkId1,
                                           mojo::MakeRequest(&frame_sink),
                                           frame_sink_client.GetInterfacePtr());
  host_manager().RegisterFrameSinkHierarchy(kFrameSinkId2, kFrameSinkId1);
  host_manager().DestroyCompositorFrameSink(kFrameSinkId1);

  // Register is called explicitly.
  EXPECT_CALL(manager_impl(),
              RegisterFrameSinkHierarchy(kFrameSinkId2, kFrameSinkId1));

  // Unregister should be called when DestroyCompositorFrameSink() is called.
  EXPECT_CALL(manager_impl(),
              UnregisterFrameSinkHierarchy(kFrameSinkId2, kFrameSinkId1))
      .WillOnce(InvokeClosure(run_loop.QuitClosure()));

  run_loop.Run();
}

}  // namespace test
}  // namespace viz
