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

constexpr FrameSinkId kParentFrameSinkId(1, 1);
constexpr FrameSinkId kClientFrameSinkId(2, 1);

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

  // Sets a callback to be called when the pipe for |binding_| is closed.
  void SetConnectionClosedCallback(base::OnceClosure callback) {
    EXPECT_TRUE(binding_.is_bound());
    binding_.set_connection_error_handler(std::move(callback));
  }

 private:
  // cc::mojom::CompositorFrameSinkClient:
  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override {}
  void OnBeginFrame(const BeginFrameArgs& begin_frame_args) override {}
  void OnBeginFramePausedChanged(bool paused) override {}
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

  bool FrameSinkDataExists(const FrameSinkId& frame_sink_id) {
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

// Verify that creating and destroying a CompositorFrameSink using
// mojom::CompositorFrameSink works correctly.
TEST_F(HostFrameSinkManagerTest, CreateMojomCompositorFrameSink) {
  base::RunLoop run_loop;

  cc::mojom::CompositorFrameSinkPtr frame_sink;
  StubCompositorFrameSinkClient frame_sink_client;
  host_manager().CreateCompositorFrameSink(kClientFrameSinkId,
                                           mojo::MakeRequest(&frame_sink),
                                           frame_sink_client.GetInterfacePtr());

  EXPECT_TRUE(FrameSinkDataExists(kClientFrameSinkId));

  // Register should call through to FrameSinkManagerImpl and should work even
  // though |kParentFrameSinkId| was not created yet.
  EXPECT_CALL(manager_impl(), RegisterFrameSinkHierarchy(kParentFrameSinkId,
                                                         kClientFrameSinkId));
  host_manager().RegisterFrameSinkHierarchy(kParentFrameSinkId,
                                            kClientFrameSinkId);

  // Destroying the CompositorFrameSink should close the client connection.
  frame_sink_client.SetConnectionClosedCallback(run_loop.QuitClosure());
  host_manager().DestroyCompositorFrameSink(kClientFrameSinkId);
  run_loop.Run();

  // We should still have the hierarchy data for |kClientFrameSinkId|.
  EXPECT_TRUE(FrameSinkDataExists(kClientFrameSinkId));

  // Unregister should work after the CompositorFrameSink is destroyed.
  EXPECT_CALL(manager_impl(), UnregisterFrameSinkHierarchy(kParentFrameSinkId,
                                                           kClientFrameSinkId));
  host_manager().UnregisterFrameSinkHierarchy(kParentFrameSinkId,
                                              kClientFrameSinkId);

  // Data for |kClientFrameSinkId| should be deleted now.
  EXPECT_FALSE(FrameSinkDataExists(kClientFrameSinkId));
}

// Verify that that creating two CompositorFrameSinkSupports work and that
// FrameSink hierarchy registration is independent of the creation.
TEST_F(HostFrameSinkManagerTest, CreateCompositorFrameSinkSupport) {
  auto support_client = host_manager().CreateCompositorFrameSinkSupport(
      nullptr /* client */, kClientFrameSinkId, false /* is_root */,
      true /* handles_frame_sink_id_invalidation */,
      false /* needs_sync_points */);
  EXPECT_TRUE(FrameSinkDataExists(kClientFrameSinkId));

  auto support_parent = host_manager().CreateCompositorFrameSinkSupport(
      nullptr /* client */, kParentFrameSinkId, true /* is_root */,
      true /* handles_frame_sink_id_invalidation */,
      false /* needs_sync_points */);
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

}  // namespace test
}  // namespace viz
