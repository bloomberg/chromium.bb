// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_frame_sink_manager.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace test {
namespace {

constexpr FrameSinkId kFrameSinkId1(1, 1);
constexpr FrameSinkId kFrameSinkId2(2, 1);

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
class MockFrameSinkManagerImpl : public FrameSinkManagerImpl {
 public:
  MockFrameSinkManagerImpl() = default;
  ~MockFrameSinkManagerImpl() override = default;

  // cc::mojom::FrameSinkManager:
  MOCK_METHOD2(RegisterFrameSinkHierarchy,
               void(const FrameSinkId& parent, const FrameSinkId& child));
  MOCK_METHOD2(UnregisterFrameSinkHierarchy,
               void(const FrameSinkId& parent, const FrameSinkId& child));
  MOCK_METHOD1(DropTemporaryReference, void(const SurfaceId& surface_id));

  // TODO(kylechar): See if we can mock functions with InterfacePtr parameters.

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

  bool FrameSinkIdExists(const FrameSinkId& frame_sink_id) {
    return host_manager_->frame_sink_data_map_.count(frame_sink_id) > 0;
  }

  // testing::Test:
  void SetUp() override {
    manager_impl_ = base::MakeUnique<MockFrameSinkManagerImpl>();
    host_manager_ = base::MakeUnique<HostFrameSinkManager>();

    manager_impl_->SetLocalClient(host_manager_.get());
    host_manager_->SetLocalManager(manager_impl_.get());
  }

 private:
  std::unique_ptr<HostFrameSinkManager> host_manager_;
  std::unique_ptr<MockFrameSinkManagerImpl> manager_impl_;

  DISALLOW_COPY_AND_ASSIGN(HostFrameSinkManagerTest);
};

// Verify that when destroying a CompositorFrameSink with registered FrameSink
// hierarchy, the hierarchy is automatically unregistered.
TEST_F(HostFrameSinkManagerTest, UnregisterHierarchyOnDestroy) {
  // Register is called explicitly.
  EXPECT_CALL(manager_impl(),
              RegisterFrameSinkHierarchy(kFrameSinkId2, kFrameSinkId1));

  // Unregister should be called when DestroyCompositorFrameSink() is called.
  EXPECT_CALL(manager_impl(),
              UnregisterFrameSinkHierarchy(kFrameSinkId2, kFrameSinkId1));

  cc::mojom::CompositorFrameSinkPtr frame_sink;
  StubCompositorFrameSinkClient frame_sink_client;
  host_manager().CreateCompositorFrameSink(kFrameSinkId1,
                                           mojo::MakeRequest(&frame_sink),
                                           frame_sink_client.GetInterfacePtr());
  host_manager().RegisterFrameSinkHierarchy(kFrameSinkId2, kFrameSinkId1);
  host_manager().DestroyCompositorFrameSink(kFrameSinkId1);
}

// Checks that creating a CompositorFrameSinkSupport registers it and destroying
// the CompositorFrameSinkSupport unregisters it.
TEST_F(HostFrameSinkManagerTest, CreateDestroyCompositorFrameSinkSupport) {
  auto support = host_manager().CreateCompositorFrameSinkSupport(
      nullptr /* client */, kFrameSinkId1, true /* is_root */,
      true /* handles_frame_sink_id_invalidation */,
      false /* needs_sync_points */);
  EXPECT_TRUE(FrameSinkIdExists(kFrameSinkId1));

  support.reset();
  EXPECT_FALSE(FrameSinkIdExists(kFrameSinkId1));
}

}  // namespace test
}  // namespace viz
