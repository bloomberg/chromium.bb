// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/frame_sink_manager_host.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "cc/surfaces/frame_sink_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace test {
namespace {

constexpr cc::FrameSinkId kFrameSinkId1(1, 1);
constexpr cc::FrameSinkId kFrameSinkId2(1, 1);

ACTION_P(InvokeClosure, closure) {
  closure.Run();
}

// A stub MojoCompositorFrameSinkClient that does nothing.
class StubClientCompositorFrameSink
    : public cc::mojom::MojoCompositorFrameSinkClient {
 public:
  StubClientCompositorFrameSink() : binding_(this) {}
  ~StubClientCompositorFrameSink() override = default;

  cc::mojom::MojoCompositorFrameSinkClientPtr GetInterfacePtr() {
    cc::mojom::MojoCompositorFrameSinkClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client));
    return client;
  }

 private:
  // cc::mojom::MojoCompositorFrameSinkClient:
  void DidReceiveCompositorFrameAck(
      const cc::ReturnedResourceArray& resources) override {}
  void OnBeginFrame(const cc::BeginFrameArgs& begin_frame_args) override {}
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override {}

  mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(StubClientCompositorFrameSink);
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
      const cc::FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      cc::mojom::MojoCompositorFrameSinkAssociatedRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateAssociatedRequest display_private_request)
      override {}
  void CreateCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client) override {}
  MOCK_METHOD2(RegisterFrameSinkHierarchy,
               void(const cc::FrameSinkId& parent,
                    const cc::FrameSinkId& child));
  MOCK_METHOD2(UnregisterFrameSinkHierarchy,
               void(const cc::FrameSinkId& parent,
                    const cc::FrameSinkId& child));
  MOCK_METHOD1(DropTemporaryReference, void(const cc::SurfaceId& surface_id));

 private:
  mojo::Binding<cc::mojom::FrameSinkManager> binding_;
  cc::mojom::FrameSinkManagerClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(MockFrameSinkManagerImpl);
};

}  // namespace

class FrameSinkManagerHostTest : public testing::Test {
 public:
  FrameSinkManagerHostTest() = default;
  ~FrameSinkManagerHostTest() override = default;

  FrameSinkManagerHost& manager_host() { return *manager_host_; }

  MockFrameSinkManagerImpl& manager_impl() { return *manager_impl_; }

  // testing::Test:
  void SetUp() override {
    manager_impl_ = base::MakeUnique<MockFrameSinkManagerImpl>();
    manager_host_ = base::MakeUnique<FrameSinkManagerHost>();

    // Connect FrameSinkManagerHost and MojoFrameSinkManager.
    cc::mojom::FrameSinkManagerClientPtr host_mojo;
    cc::mojom::FrameSinkManagerClientRequest host_mojo_request =
        mojo::MakeRequest(&host_mojo);
    cc::mojom::FrameSinkManagerPtr manager_mojo;
    cc::mojom::FrameSinkManagerRequest manager_impl_request =
        mojo::MakeRequest(&manager_mojo);
    manager_impl_->BindAndSetClient(std::move(manager_impl_request),
                                    std::move(host_mojo));
    manager_host_->BindManagerClientAndSetManagerPtr(
        std::move(host_mojo_request), std::move(manager_mojo));
  }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<FrameSinkManagerHost> manager_host_;
  std::unique_ptr<MockFrameSinkManagerImpl> manager_impl_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkManagerHostTest);
};

// Verify that when destroying a CompositorFrameSink with registered FrameSink
// hierarchy, the hierarchy is automatically unregistered.
TEST_F(FrameSinkManagerHostTest, UnregisterHierarchyOnDestroy) {
  base::RunLoop run_loop;

  cc::mojom::MojoCompositorFrameSinkPtr frame_sink;
  StubClientCompositorFrameSink frame_sink_client;
  manager_host().CreateCompositorFrameSink(kFrameSinkId1,
                                           mojo::MakeRequest(&frame_sink),
                                           frame_sink_client.GetInterfacePtr());
  manager_host().RegisterFrameSinkHierarchy(kFrameSinkId2, kFrameSinkId1);
  manager_host().DestroyCompositorFrameSink(kFrameSinkId1);

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
