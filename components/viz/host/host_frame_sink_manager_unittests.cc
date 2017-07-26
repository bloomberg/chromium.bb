// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_frame_sink_manager.h"

#include <memory>
#include <utility>

#include "base/macros.h"
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

// A mock implementation of mojom::FrameSinkManager.
class MockFrameSinkManagerImpl : public FrameSinkManagerImpl {
 public:
  MockFrameSinkManagerImpl() = default;
  ~MockFrameSinkManagerImpl() override = default;

  // cc::mojom::FrameSinkManager:
  MOCK_METHOD1(RegisterFrameSinkId, void(const FrameSinkId& frame_sink_id));
  MOCK_METHOD1(InvalidateFrameSinkId, void(const FrameSinkId& frame_sink_id));
  // Work around for gmock not supporting move-only types.
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      cc::mojom::CompositorFrameSinkRequest request,
      cc::mojom::CompositorFrameSinkClientPtr client) override {
    MockCreateCompositorFrameSink(frame_sink_id);
  }
  MOCK_METHOD1(MockCreateCompositorFrameSink,
               void(const FrameSinkId& frame_sink_id));
  MOCK_METHOD2(RegisterFrameSinkHierarchy,
               void(const FrameSinkId& parent, const FrameSinkId& child));
  MOCK_METHOD2(UnregisterFrameSinkHierarchy,
               void(const FrameSinkId& parent, const FrameSinkId& child));
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
  // Calling CreateCompositorFrameSink() should first register the frame sink
  // and then request to create it.
  EXPECT_CALL(manager_impl(), RegisterFrameSinkId(kClientFrameSinkId));
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
  host_manager().DestroyCompositorFrameSink(kClientFrameSinkId);

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

// Verify that that creating two CompositorFrameSinkSupports works.
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
