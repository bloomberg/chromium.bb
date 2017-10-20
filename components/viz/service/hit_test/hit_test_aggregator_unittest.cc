// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/hit_test/hit_test_aggregator.h"

#include <map>

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/hit_test/hit_test_aggregator_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

constexpr FrameSinkId kDisplayFrameSink(2, 0);

SurfaceId MakeSurfaceId(const FrameSinkId& frame_sink_id, uint32_t local_id) {
  return SurfaceId(
      frame_sink_id,
      LocalSurfaceId(local_id, base::UnguessableToken::Deserialize(0, 1u)));
}

}  // namespace

class TestHitTestAggregator final : public HitTestAggregator {
 public:
  TestHitTestAggregator(HitTestManager* manager,
                        HitTestAggregatorDelegate* delegate)
      : HitTestAggregator(manager, delegate) {}
  ~TestHitTestAggregator() = default;

  int GetRegionCount() const {
    AggregatedHitTestRegion* start =
        static_cast<AggregatedHitTestRegion*>(read_buffer_.get());
    AggregatedHitTestRegion* end = start;
    while (end->child_count != kEndOfList)
      end++;
    return end - start;
  }
  int GetHitTestRegionListSize() { return read_size_; }
  void SwapHandles() {
    delegate_->SwitchActiveAggregatedHitTestRegionList(active_handle_index_);
  }

  void Reset() {
    AggregatedHitTestRegion* regions =
        static_cast<AggregatedHitTestRegion*>(write_buffer_.get());
    regions[0].child_count = kEndOfList;

    regions = static_cast<AggregatedHitTestRegion*>(read_buffer_.get());
    regions[0].child_count = kEndOfList;
  }
};

namespace {

class TestFrameSinkManagerImpl;

}  // namespace

class TestHitTestManager : public HitTestManager {
 public:
  explicit TestHitTestManager(TestFrameSinkManagerImpl* frame_sink_manager);
  ~TestHitTestManager() override = default;

  void CallOnSurfaceActivated(const SurfaceId surface_id) {
    OnSurfaceActivated(surface_id);
  }
  void CallOnSurfaceDiscarded(const SurfaceId surface_id) {
    OnSurfaceDiscarded(surface_id);
  }
  void Reset() { hit_test_region_lists_.clear(); }

  int GetRegionCount() {
    int count = 0;
    for (auto& i : hit_test_region_lists_) {
      count += i.second.size();
    }
    return count;
  }
};

namespace {

class TestGpuRootCompositorFrameSink : public HitTestAggregatorDelegate {
 public:
  TestGpuRootCompositorFrameSink(TestHitTestManager* hit_test_manager,
                                 TestFrameSinkManagerImpl* frame_sink_manager,
                                 const FrameSinkId& frame_sink_id)
      : frame_sink_manager_(frame_sink_manager),
        frame_sink_id_(frame_sink_id),
        aggregator_(
            base::MakeUnique<TestHitTestAggregator>(hit_test_manager, this)) {}
  ~TestGpuRootCompositorFrameSink() override = default;

  // HitTestAggregatorDelegate:
  void OnAggregatedHitTestRegionListUpdated(
      mojo::ScopedSharedBufferHandle active_handle,
      uint32_t active_handle_size,
      mojo::ScopedSharedBufferHandle idle_handle,
      uint32_t idle_handle_size) override;
  void SwitchActiveAggregatedHitTestRegionList(
      uint8_t active_handle_index) override;

  TestHitTestAggregator* aggregator() { return aggregator_.get(); }

 private:
  TestFrameSinkManagerImpl* frame_sink_manager_;
  FrameSinkId frame_sink_id_;
  std::unique_ptr<TestHitTestAggregator> aggregator_;

  DISALLOW_COPY_AND_ASSIGN(TestGpuRootCompositorFrameSink);
};

class TestHostFrameSinkManager : public HostFrameSinkManager {
 public:
  TestHostFrameSinkManager() = default;
  ~TestHostFrameSinkManager() override = default;

  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      mojo::ScopedSharedBufferHandle active_handle,
      uint32_t active_handle_size,
      mojo::ScopedSharedBufferHandle idle_handle,
      uint32_t idle_handle_size) override {
    DCHECK(active_handle.is_valid() && idle_handle.is_valid());
    buffer_frame_sink_id_ = frame_sink_id;
    handle_buffers_[0] = active_handle->Map(active_handle_size *
                                            sizeof(AggregatedHitTestRegion));
    handle_buffers_[1] =
        idle_handle->Map(idle_handle_size * sizeof(AggregatedHitTestRegion));
    SwitchActiveAggregatedHitTestRegionList(buffer_frame_sink_id_, 0);
  }

  void SwitchActiveAggregatedHitTestRegionList(
      const FrameSinkId& frame_sink_id,
      uint8_t active_handle_index) override {
    active_list_ = static_cast<AggregatedHitTestRegion*>(
        handle_buffers_[active_handle_index].get());
  }

  AggregatedHitTestRegion* regions() { return active_list_; }

  const FrameSinkId& buffer_frame_sink_id() { return buffer_frame_sink_id_; }

 private:
  FrameSinkId buffer_frame_sink_id_;
  mojo::ScopedSharedBufferMapping handle_buffers_[2];
  AggregatedHitTestRegion* active_list_;

  DISALLOW_COPY_AND_ASSIGN(TestHostFrameSinkManager);
};

class TestFrameSinkManagerImpl : public FrameSinkManagerImpl {
 public:
  TestFrameSinkManagerImpl() = default;
  ~TestFrameSinkManagerImpl() override = default;

  void SetLocalClient(TestHostFrameSinkManager* client) {
    host_client_ = client;
  }

  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      mojo::ScopedSharedBufferHandle active_handle,
      uint32_t active_handle_size,
      mojo::ScopedSharedBufferHandle idle_handle,
      uint32_t idle_handle_size) {
    // Do not check if it's on valid thread for tests.
    if (host_client_) {
      host_client_->OnAggregatedHitTestRegionListUpdated(
          frame_sink_id, std::move(active_handle), active_handle_size,
          std::move(idle_handle), idle_handle_size);
    }
  }

  void SwitchActiveAggregatedHitTestRegionList(const FrameSinkId& frame_sink_id,
                                               uint8_t active_handle_index) {
    // Do not check if it's on valid thread for tests.
    if (host_client_) {
      host_client_->SwitchActiveAggregatedHitTestRegionList(
          frame_sink_id, active_handle_index);
    }
  }

  void CreateRootCompositorFrameSinkLocal(TestHitTestManager* hit_test_manager,
                                          const FrameSinkId& frame_sink_id) {
    compositor_frame_sinks_[frame_sink_id] =
        base::MakeUnique<TestGpuRootCompositorFrameSink>(hit_test_manager, this,
                                                         frame_sink_id);
  }

  const std::map<FrameSinkId, std::unique_ptr<TestGpuRootCompositorFrameSink>>&
  compositor_frame_sinks() {
    return compositor_frame_sinks_;
  }

  uint64_t GetActiveFrameIndex(const SurfaceId& surface_id) override {
    return 0;
  }

 private:
  std::map<FrameSinkId, std::unique_ptr<TestGpuRootCompositorFrameSink>>
      compositor_frame_sinks_;
  TestHostFrameSinkManager* host_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestFrameSinkManagerImpl);
};

void TestGpuRootCompositorFrameSink::OnAggregatedHitTestRegionListUpdated(
    mojo::ScopedSharedBufferHandle active_handle,
    uint32_t active_handle_size,
    mojo::ScopedSharedBufferHandle idle_handle,
    uint32_t idle_handle_size) {
  frame_sink_manager_->OnAggregatedHitTestRegionListUpdated(
      frame_sink_id_, std::move(active_handle), active_handle_size,
      std::move(idle_handle), idle_handle_size);
}

void TestGpuRootCompositorFrameSink::SwitchActiveAggregatedHitTestRegionList(
    uint8_t active_handle_index) {
  frame_sink_manager_->SwitchActiveAggregatedHitTestRegionList(
      frame_sink_id_, active_handle_index);
}

}  // namespace

TestHitTestManager::TestHitTestManager(
    TestFrameSinkManagerImpl* frame_sink_manager)
    : HitTestManager(frame_sink_manager) {}

class HitTestAggregatorTest : public testing::Test {
 public:
  HitTestAggregatorTest() = default;
  ~HitTestAggregatorTest() override = default;

  // testing::Test:
  void SetUp() override {
    frame_sink_manager_ = base::MakeUnique<TestFrameSinkManagerImpl>();
    host_frame_sink_manager_ = base::MakeUnique<TestHostFrameSinkManager>();
    hit_test_manager_ =
        base::MakeUnique<TestHitTestManager>(frame_sink_manager_.get());
    frame_sink_manager_->SetLocalClient(host_frame_sink_manager_.get());
    frame_sink_manager_->CreateRootCompositorFrameSinkLocal(
        hit_test_manager_.get(), kDisplayFrameSink);
  }
  void TearDown() override {
    frame_sink_manager_.reset();
    host_frame_sink_manager_.reset();
    hit_test_manager_.reset();
  }

  // Creates a hit test data element with 8 children recursively to
  // the specified depth.  SurfaceIds are generated in sequential order and
  // the method returns the next unused id.
  int CreateAndSubmitHitTestRegionListWith8Children(int id, int depth) {
    SurfaceId surface_id = MakeSurfaceId(kDisplayFrameSink, id);
    id++;

    auto hit_test_region_list = mojom::HitTestRegionList::New();
    hit_test_region_list->flags = mojom::kHitTestMine;
    hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

    for (int i = 0; i < 8; i++) {
      auto hit_test_region = mojom::HitTestRegion::New();
      hit_test_region->rect.SetRect(100, 100, 100, 100);
      SurfaceId child_surface_id = MakeSurfaceId(kDisplayFrameSink, id);
      hit_test_region->frame_sink_id = child_surface_id.frame_sink_id();
      hit_test_region->local_surface_id = child_surface_id.local_surface_id();

      if (depth > 0) {
        hit_test_region->flags = mojom::kHitTestChildSurface;
        id = CreateAndSubmitHitTestRegionListWith8Children(id, depth - 1);
      } else {
        hit_test_region->flags = mojom::kHitTestMine;
      }
      hit_test_region_list->regions.push_back(std::move(hit_test_region));
    }

    hit_test_manager()->SubmitHitTestRegionList(
        surface_id, 0, std::move(hit_test_region_list));
    return id;
  }

 protected:
  TestHitTestAggregator* GetAggregator(const FrameSinkId& frame_sink_id) {
    DCHECK(frame_sink_manager_->compositor_frame_sinks().count(frame_sink_id));
    return frame_sink_manager_->compositor_frame_sinks()
        .at(frame_sink_id)
        ->aggregator();
  }

  AggregatedHitTestRegion* host_regions() {
    return host_frame_sink_manager_->regions();
  }

  const FrameSinkId& host_buffer_frame_sink_id() {
    return host_frame_sink_manager_->buffer_frame_sink_id();
  }

  TestHitTestManager* hit_test_manager() { return hit_test_manager_.get(); }

 private:
  std::unique_ptr<TestHitTestManager> hit_test_manager_;
  std::unique_ptr<TestFrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<TestHostFrameSinkManager> host_frame_sink_manager_;

  DISALLOW_COPY_AND_ASSIGN(HitTestAggregatorTest);
};

// TODO(gklassen): Add tests for 3D use cases as suggested by and with
// input from rjkroege.

// One surface.
//
//  +----------+
//  |          |
//  |          |
//  |          |
//  +----------+
//
TEST_F(HitTestAggregatorTest, OneSurface) {
  TestHitTestAggregator* aggregator = GetAggregator(kDisplayFrameSink);
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId display_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);

  auto hit_test_region_list = mojom::HitTestRegionList::New();
  hit_test_region_list->flags = mojom::kHitTestMine;
  hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

  hit_test_manager()->SubmitHitTestRegionList(display_surface_id, 0,
                                              std::move(hit_test_region_list));
  aggregator->Aggregate(display_surface_id);
  aggregator->SwapHandles();

  // Expect 1 entry routing all events to the one surface (display root).
  EXPECT_EQ(aggregator->GetRegionCount(), 1);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);
  AggregatedHitTestRegion* regions = host_regions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, display_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region->child_count, 0);
}

// One opaque embedder with two regions.
//
//  +e-------------+
//  | +r1-+ +r2--+ |
//  | |   | |    | |
//  | |   | |    | |
//  | +---+ +----+ |
//  +--------------+
//
TEST_F(HitTestAggregatorTest, OneEmbedderTwoRegions) {
  TestHitTestAggregator* aggregator = GetAggregator(kDisplayFrameSink);
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);

  auto e_hit_test_region_list = mojom::HitTestRegionList::New();
  e_hit_test_region_list->flags = mojom::kHitTestMine;
  e_hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_r1 = mojom::HitTestRegion::New();
  e_hit_test_region_r1->frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_r1->local_surface_id = e_surface_id.local_surface_id();
  e_hit_test_region_r1->flags = mojom::kHitTestMine;
  e_hit_test_region_r1->rect.SetRect(100, 100, 200, 400);

  auto e_hit_test_region_r2 = mojom::HitTestRegion::New();
  e_hit_test_region_r2->frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_r2->local_surface_id = e_surface_id.local_surface_id();
  e_hit_test_region_r2->flags = mojom::kHitTestMine;
  e_hit_test_region_r2->rect.SetRect(400, 100, 300, 400);

  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_r1));
  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_r2));

  // Submit in unexpected order.

  hit_test_manager()->SubmitHitTestRegionList(
      e_surface_id, 0, std::move(e_hit_test_region_list));

  aggregator->Aggregate(e_surface_id);
  aggregator->SwapHandles();

  EXPECT_EQ(aggregator->GetRegionCount(), 3);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);
  AggregatedHitTestRegion* regions = host_regions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region->child_count, 2);

  region = &regions[1];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->rect, gfx::Rect(100, 100, 200, 400));
  EXPECT_EQ(region->child_count, 0);

  region = &regions[2];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->rect, gfx::Rect(400, 100, 300, 400));
  EXPECT_EQ(region->child_count, 0);
}

// One embedder with two children.
//
//  +e-------------+
//  | +c1-+ +c2--+ |
//  | |   | |    | |
//  | |   | |    | |
//  | +---+ +----+ |
//  +--------------+
//

TEST_F(HitTestAggregatorTest, OneEmbedderTwoChildren) {
  TestHitTestAggregator* aggregator = GetAggregator(kDisplayFrameSink);
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c1_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);
  SurfaceId c2_surface_id = MakeSurfaceId(kDisplayFrameSink, 3);

  auto e_hit_test_region_list = mojom::HitTestRegionList::New();
  e_hit_test_region_list->flags = mojom::kHitTestMine;
  e_hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_c1 = mojom::HitTestRegion::New();
  e_hit_test_region_c1->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c1->frame_sink_id = c1_surface_id.frame_sink_id();
  e_hit_test_region_c1->local_surface_id = c1_surface_id.local_surface_id();
  e_hit_test_region_c1->rect.SetRect(100, 100, 200, 300);

  auto e_hit_test_region_c2 = mojom::HitTestRegion::New();
  e_hit_test_region_c2->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c2->frame_sink_id = c2_surface_id.frame_sink_id();
  e_hit_test_region_c2->local_surface_id = c2_surface_id.local_surface_id();
  e_hit_test_region_c2->rect.SetRect(400, 100, 400, 300);

  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_c1));
  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_c2));

  auto c1_hit_test_region_list = mojom::HitTestRegionList::New();

  auto c2_hit_test_region_list = mojom::HitTestRegionList::New();

  // Submit in unexpected order.

  hit_test_manager()->SubmitHitTestRegionList(
      c1_surface_id, 0, std::move(c1_hit_test_region_list));
  hit_test_manager()->SubmitHitTestRegionList(
      e_surface_id, 0, std::move(e_hit_test_region_list));
  hit_test_manager()->SubmitHitTestRegionList(
      c2_surface_id, 0, std::move(c2_hit_test_region_list));

  aggregator->Aggregate(e_surface_id);
  aggregator->SwapHandles();

  EXPECT_EQ(aggregator->GetRegionCount(), 3);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);
  AggregatedHitTestRegion* regions = host_regions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region->child_count, 2);

  region = &regions[1];
  EXPECT_EQ(region->flags, mojom::kHitTestChildSurface);
  EXPECT_EQ(region->frame_sink_id, c1_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(100, 100, 200, 300));
  EXPECT_EQ(region->child_count, 0);

  region = &regions[2];
  EXPECT_EQ(region->flags, mojom::kHitTestChildSurface);
  EXPECT_EQ(region->frame_sink_id, c2_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(400, 100, 400, 300));
  EXPECT_EQ(region->child_count, 0);
}

// Occluded child frame (OOPIF).
//
//  +e-----------+
//  | +c--+      |
//  | | +div-+   |
//  | | |    |   |
//  | | +----+   |
//  | +---+      |
//  +------------+
//

TEST_F(HitTestAggregatorTest, OccludedChildFrame) {
  TestHitTestAggregator* aggregator = GetAggregator(kDisplayFrameSink);
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);

  auto e_hit_test_region_list = mojom::HitTestRegionList::New();
  e_hit_test_region_list->flags = mojom::kHitTestMine;
  e_hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_div = mojom::HitTestRegion::New();
  e_hit_test_region_div->flags = mojom::kHitTestMine;
  e_hit_test_region_div->frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_div->local_surface_id = e_surface_id.local_surface_id();
  e_hit_test_region_div->rect.SetRect(200, 200, 300, 200);

  auto e_hit_test_region_c = mojom::HitTestRegion::New();
  e_hit_test_region_c->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c->frame_sink_id = c_surface_id.frame_sink_id();
  e_hit_test_region_c->local_surface_id = c_surface_id.local_surface_id();
  e_hit_test_region_c->rect.SetRect(100, 100, 200, 500);

  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_div));
  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_c));

  auto c_hit_test_region_list = mojom::HitTestRegionList::New();
  c_hit_test_region_list->flags = mojom::kHitTestMine;
  c_hit_test_region_list->bounds.SetRect(0, 0, 200, 500);

  // Submit in unexpected order.

  hit_test_manager()->SubmitHitTestRegionList(
      c_surface_id, 0, std::move(c_hit_test_region_list));
  hit_test_manager()->SubmitHitTestRegionList(
      e_surface_id, 0, std::move(e_hit_test_region_list));

  aggregator->Aggregate(e_surface_id);
  aggregator->SwapHandles();

  EXPECT_EQ(aggregator->GetRegionCount(), 3);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);
  AggregatedHitTestRegion* regions = host_regions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region->child_count, 2);

  region = &regions[1];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(200, 200, 300, 200));
  EXPECT_EQ(region->child_count, 0);

  region = &regions[2];
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, region->flags);
  EXPECT_EQ(region->frame_sink_id, c_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(100, 100, 200, 500));
  EXPECT_EQ(region->child_count, 0);
}

// Foreground child frame (OOPIF).
// Same as the previous test except the child is foreground.
//
//  +e-----------+
//  | +c--+      |
//  | |   |div-+ |
//  | |   |    | |
//  | |   |----+ |
//  | +---+      |
//  +------------+
//

TEST_F(HitTestAggregatorTest, ForegroundChildFrame) {
  TestHitTestAggregator* aggregator = GetAggregator(kDisplayFrameSink);
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);

  auto e_hit_test_region_list = mojom::HitTestRegionList::New();
  e_hit_test_region_list->flags = mojom::kHitTestMine;
  e_hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_div = mojom::HitTestRegion::New();
  e_hit_test_region_div->flags = mojom::kHitTestMine;
  e_hit_test_region_div->frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_div->local_surface_id = e_surface_id.local_surface_id();
  e_hit_test_region_div->rect.SetRect(200, 200, 300, 200);

  auto e_hit_test_region_c = mojom::HitTestRegion::New();
  e_hit_test_region_c->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c->frame_sink_id = c_surface_id.frame_sink_id();
  e_hit_test_region_c->local_surface_id = c_surface_id.local_surface_id();
  e_hit_test_region_c->rect.SetRect(100, 100, 200, 500);

  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_c));
  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_div));

  auto c_hit_test_region_list = mojom::HitTestRegionList::New();
  c_hit_test_region_list->flags = mojom::kHitTestMine;
  c_hit_test_region_list->bounds.SetRect(0, 0, 200, 500);

  // Submit in unexpected order.

  hit_test_manager()->SubmitHitTestRegionList(
      c_surface_id, 0, std::move(c_hit_test_region_list));
  hit_test_manager()->SubmitHitTestRegionList(
      e_surface_id, 0, std::move(e_hit_test_region_list));

  aggregator->Aggregate(e_surface_id);
  aggregator->SwapHandles();

  EXPECT_EQ(aggregator->GetRegionCount(), 3);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);
  AggregatedHitTestRegion* regions = host_regions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region->child_count, 2);

  region = &regions[1];
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, region->flags);
  EXPECT_EQ(region->frame_sink_id, c_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(100, 100, 200, 500));
  EXPECT_EQ(region->child_count, 0);

  region = &regions[2];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(200, 200, 300, 200));
  EXPECT_EQ(region->child_count, 0);
}

// One embedder with a clipped child with a tab and transparent background.
//
//  +e-------------+
//  |   +c---------|     Point   maps to
//  | 1 |+a--+     |     -----   -------
//  |   || 2 |  3  |       1        e
//  |   |+b--------|       2        a
//  |   ||         |       3        e (transparent area in c)
//  |   ||   4     |       4        b
//  +--------------+
//

TEST_F(HitTestAggregatorTest, ClippedChildWithTabAndTransparentBackground) {
  TestHitTestAggregator* aggregator = GetAggregator(kDisplayFrameSink);
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);
  SurfaceId a_surface_id = MakeSurfaceId(kDisplayFrameSink, 3);
  SurfaceId b_surface_id = MakeSurfaceId(kDisplayFrameSink, 4);

  auto e_hit_test_region_list = mojom::HitTestRegionList::New();
  e_hit_test_region_list->flags = mojom::kHitTestMine;
  e_hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_c = mojom::HitTestRegion::New();
  e_hit_test_region_c->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c->frame_sink_id = c_surface_id.frame_sink_id();
  e_hit_test_region_c->local_surface_id = c_surface_id.local_surface_id();
  e_hit_test_region_c->rect.SetRect(300, 100, 1600, 800);
  e_hit_test_region_c->transform.Translate(200, 100);

  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_c));

  auto c_hit_test_region_list = mojom::HitTestRegionList::New();
  c_hit_test_region_list->flags = mojom::kHitTestIgnore;
  c_hit_test_region_list->bounds.SetRect(0, 0, 1600, 800);

  auto c_hit_test_region_a = mojom::HitTestRegion::New();
  c_hit_test_region_a->flags = mojom::kHitTestChildSurface;
  c_hit_test_region_a->frame_sink_id = a_surface_id.frame_sink_id();
  c_hit_test_region_a->local_surface_id = a_surface_id.local_surface_id();
  c_hit_test_region_a->rect.SetRect(0, 0, 200, 100);

  auto c_hit_test_region_b = mojom::HitTestRegion::New();
  c_hit_test_region_b->flags = mojom::kHitTestChildSurface;
  c_hit_test_region_b->frame_sink_id = b_surface_id.frame_sink_id();
  c_hit_test_region_b->local_surface_id = b_surface_id.local_surface_id();
  c_hit_test_region_b->rect.SetRect(0, 100, 800, 600);

  c_hit_test_region_list->regions.push_back(std::move(c_hit_test_region_a));
  c_hit_test_region_list->regions.push_back(std::move(c_hit_test_region_b));

  auto a_hit_test_region_list = mojom::HitTestRegionList::New();
  a_hit_test_region_list->flags = mojom::kHitTestMine;
  a_hit_test_region_list->bounds.SetRect(0, 0, 200, 100);

  auto b_hit_test_region_list = mojom::HitTestRegionList::New();
  b_hit_test_region_list->flags = mojom::kHitTestMine;
  b_hit_test_region_list->bounds.SetRect(0, 100, 800, 600);

  // Submit in unexpected order.

  hit_test_manager()->SubmitHitTestRegionList(
      c_surface_id, 0, std::move(c_hit_test_region_list));
  hit_test_manager()->SubmitHitTestRegionList(
      a_surface_id, 0, std::move(a_hit_test_region_list));
  hit_test_manager()->SubmitHitTestRegionList(
      b_surface_id, 0, std::move(b_hit_test_region_list));
  hit_test_manager()->SubmitHitTestRegionList(
      e_surface_id, 0, std::move(e_hit_test_region_list));

  aggregator->Aggregate(e_surface_id);
  aggregator->SwapHandles();

  EXPECT_EQ(aggregator->GetRegionCount(), 4);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);
  AggregatedHitTestRegion* regions = host_regions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region->child_count, 3);

  region = &regions[1];
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestIgnore, region->flags);
  EXPECT_EQ(region->frame_sink_id, c_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(300, 100, 1600, 800));
  EXPECT_EQ(region->child_count, 2);

  gfx::Point point(300, 300);
  gfx::Transform transform(region->transform);
  transform.TransformPointReverse(&point);
  EXPECT_TRUE(point == gfx::Point(100, 200));

  region = &regions[2];
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, region->flags);
  EXPECT_EQ(region->frame_sink_id, a_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(0, 0, 200, 100));
  EXPECT_EQ(region->child_count, 0);

  region = &regions[3];
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, region->flags);
  EXPECT_EQ(region->frame_sink_id, b_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(0, 100, 800, 600));
  EXPECT_EQ(region->child_count, 0);
}

// Three children deep.
//
//  +e------------+
//  | +c1-------+ |
//  | | +c2---+ | |
//  | | | +c3-| | |
//  | | | |   | | |
//  | | | +---| | |
//  | | +-----+ | |
//  | +---------+ |
//  +-------------+
//

TEST_F(HitTestAggregatorTest, ThreeChildrenDeep) {
  TestHitTestAggregator* aggregator = GetAggregator(kDisplayFrameSink);
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c1_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);
  SurfaceId c2_surface_id = MakeSurfaceId(kDisplayFrameSink, 3);
  SurfaceId c3_surface_id = MakeSurfaceId(kDisplayFrameSink, 4);

  auto e_hit_test_region_list = mojom::HitTestRegionList::New();
  e_hit_test_region_list->flags = mojom::kHitTestMine;
  e_hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_c1 = mojom::HitTestRegion::New();
  e_hit_test_region_c1->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c1->frame_sink_id = c1_surface_id.frame_sink_id();
  e_hit_test_region_c1->local_surface_id = c1_surface_id.local_surface_id();
  e_hit_test_region_c1->rect.SetRect(100, 100, 700, 700);

  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_c1));

  auto c1_hit_test_region_list = mojom::HitTestRegionList::New();
  c1_hit_test_region_list->flags = mojom::kHitTestMine;
  c1_hit_test_region_list->bounds.SetRect(0, 0, 600, 600);

  auto c1_hit_test_region_c2 = mojom::HitTestRegion::New();
  c1_hit_test_region_c2->flags = mojom::kHitTestChildSurface;
  c1_hit_test_region_c2->frame_sink_id = c2_surface_id.frame_sink_id();
  c1_hit_test_region_c2->local_surface_id = c2_surface_id.local_surface_id();
  c1_hit_test_region_c2->rect.SetRect(100, 100, 500, 500);

  c1_hit_test_region_list->regions.push_back(std::move(c1_hit_test_region_c2));

  auto c2_hit_test_region_list = mojom::HitTestRegionList::New();
  c2_hit_test_region_list->flags = mojom::kHitTestMine;
  c2_hit_test_region_list->bounds.SetRect(0, 0, 400, 400);

  auto c2_hit_test_region_c3 = mojom::HitTestRegion::New();
  c2_hit_test_region_c3->flags = mojom::kHitTestChildSurface;
  c2_hit_test_region_c3->frame_sink_id = c3_surface_id.frame_sink_id();
  c2_hit_test_region_c3->local_surface_id = c3_surface_id.local_surface_id();
  c2_hit_test_region_c3->rect.SetRect(100, 100, 300, 300);

  c2_hit_test_region_list->regions.push_back(std::move(c2_hit_test_region_c3));

  auto c3_hit_test_region_list = mojom::HitTestRegionList::New();
  c3_hit_test_region_list->flags = mojom::kHitTestMine;
  c3_hit_test_region_list->bounds.SetRect(0, 0, 200, 200);

  // Submit in unexpected order.

  hit_test_manager()->SubmitHitTestRegionList(
      c1_surface_id, 0, std::move(c1_hit_test_region_list));
  hit_test_manager()->SubmitHitTestRegionList(
      c3_surface_id, 0, std::move(c3_hit_test_region_list));
  hit_test_manager()->SubmitHitTestRegionList(
      e_surface_id, 0, std::move(e_hit_test_region_list));
  hit_test_manager()->SubmitHitTestRegionList(
      c2_surface_id, 0, std::move(c2_hit_test_region_list));

  aggregator->Aggregate(e_surface_id);
  aggregator->SwapHandles();

  EXPECT_EQ(aggregator->GetRegionCount(), 4);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);
  AggregatedHitTestRegion* regions = host_regions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region->child_count, 3);

  region = &regions[1];
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, region->flags);
  EXPECT_EQ(region->frame_sink_id, c1_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(100, 100, 700, 700));
  EXPECT_EQ(region->child_count, 2);

  region = &regions[2];
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, region->flags);
  EXPECT_EQ(region->frame_sink_id, c2_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(100, 100, 500, 500));
  EXPECT_EQ(region->child_count, 1);

  region = &regions[3];
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, region->flags);
  EXPECT_EQ(region->frame_sink_id, c3_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(100, 100, 300, 300));
  EXPECT_EQ(region->child_count, 0);
}

// Missing / late child.
//
//  +e-----------+
//  | +c--+      |
//  | |   |div-+ |
//  | |   |    | |
//  | |   |----+ |
//  | +---+      |
//  +------------+
//

TEST_F(HitTestAggregatorTest, MissingChildFrame) {
  TestHitTestAggregator* aggregator = GetAggregator(kDisplayFrameSink);
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);

  auto e_hit_test_region_list = mojom::HitTestRegionList::New();
  e_hit_test_region_list->flags = mojom::kHitTestMine;
  e_hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_div = mojom::HitTestRegion::New();
  e_hit_test_region_div->flags = mojom::kHitTestMine;
  e_hit_test_region_div->frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_div->local_surface_id = e_surface_id.local_surface_id();
  e_hit_test_region_div->rect.SetRect(200, 200, 300, 200);

  auto e_hit_test_region_c = mojom::HitTestRegion::New();
  e_hit_test_region_c->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c->frame_sink_id = c_surface_id.frame_sink_id();
  e_hit_test_region_c->local_surface_id = c_surface_id.local_surface_id();
  e_hit_test_region_c->rect.SetRect(100, 100, 200, 500);

  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_c));
  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_div));

  auto c_hit_test_region_list = mojom::HitTestRegionList::New();
  c_hit_test_region_list->flags = mojom::kHitTestMine;
  c_hit_test_region_list->bounds.SetRect(0, 0, 200, 500);

  // Submit in unexpected order.

  hit_test_manager()->SubmitHitTestRegionList(
      e_surface_id, 0, std::move(e_hit_test_region_list));

  aggregator->Aggregate(e_surface_id);
  aggregator->SwapHandles();

  EXPECT_EQ(aggregator->GetRegionCount(), 3);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);
  AggregatedHitTestRegion* regions = host_regions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region->child_count, 2);

  // |c_hit_test_region_list| was not submitted on time, but
  // |e_hit_test_region_c| itself should still be present and can get events.
  region = &regions[1];
  EXPECT_EQ(region->flags, mojom::kHitTestChildSurface | mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, c_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(100, 100, 200, 500));
  EXPECT_EQ(region->child_count, 0);

  region = &regions[2];
  EXPECT_EQ(region->flags, mojom::kHitTestMine);
  EXPECT_EQ(region->frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region->rect, gfx::Rect(200, 200, 300, 200));
  EXPECT_EQ(region->child_count, 0);
}

// Exceed limits to ensure that bounds and resize work.
//
// A tree of embedders each with 8 children and 4 levels deep = 4096 regions.
// This will exceed initial allocation and force a resize.
//
//  +e--------------------------------------------------------+
//  | +c1----------++c2----------++c3----------++c4----------+|
//  | | +c1--------|| +c1--------|| +c1--------|| +c1--------||
//  | | | +c1-++c2-|| | +c1-++c2-|| | +c1-++c2-|| | +c1-++c2-||
//  | | | |   ||   || | |   ||   || | |   ||   || | |   ||   ||
//  | | | +---++---|| | +---++---|| | +---++---|| | +---++---||
//  | +------------++------------++------------++------------+|
//  | +c5----------++c6----------++c7----------++c8----------+|
//  | | +c1--------|| +c1--------|| +c1--------|| +c1--------||
//  | | | +c1-++c2-|| | +c1-++c2-|| | +c1-++c2-|| | +c1-++c2-||
//  | | | |   ||   || | |   ||   || | |   ||   || | |   ||   ||
//  | | | +---++---|| | +---++---|| | +---++---|| | +---++---||
//  | +------------++------------++------------++------------+|
//  +---------------------------------------------------------+
//

TEST_F(HitTestAggregatorTest, ExceedLimits) {
  TestHitTestAggregator* aggregator = GetAggregator(kDisplayFrameSink);
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  EXPECT_LT(aggregator->GetHitTestRegionListSize(), 4096);

  SurfaceId display_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);

  CreateAndSubmitHitTestRegionListWith8Children(1, 3);

  aggregator->Aggregate(display_surface_id);
  aggregator->SwapHandles();

  // Expect 4680 regions:
  //  8 children 4 levels deep 8*8*8*8 is  4096
  //  1 region for each embedder/surface +  584
  //  1 root                             +    1
  //                                      -----
  //                                       4681.
  EXPECT_GE(aggregator->GetHitTestRegionListSize(), 4681);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);
  AggregatedHitTestRegion* regions = host_regions();

  uint32_t count = 0;
  while (regions->child_count != kEndOfList) {
    regions++;
    count++;
  }
  EXPECT_EQ(count, 4681u);
}

TEST_F(HitTestAggregatorTest, DiscardedSurfaces) {
  TestHitTestAggregator* aggregator = GetAggregator(kDisplayFrameSink);
  EXPECT_EQ(hit_test_manager()->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);

  auto e_hit_test_region_list = mojom::HitTestRegionList::New();
  e_hit_test_region_list->flags = mojom::kHitTestMine;
  e_hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_div = mojom::HitTestRegion::New();
  e_hit_test_region_div->flags = mojom::kHitTestMine;
  e_hit_test_region_div->frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_div->local_surface_id = e_surface_id.local_surface_id();
  e_hit_test_region_div->rect.SetRect(200, 200, 300, 200);

  auto e_hit_test_region_c = mojom::HitTestRegion::New();
  e_hit_test_region_c->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c->frame_sink_id = c_surface_id.frame_sink_id();
  e_hit_test_region_c->local_surface_id = c_surface_id.local_surface_id();
  e_hit_test_region_c->rect.SetRect(100, 100, 200, 500);

  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_c));
  e_hit_test_region_list->regions.push_back(std::move(e_hit_test_region_div));

  auto c_hit_test_region_list = mojom::HitTestRegionList::New();
  c_hit_test_region_list->flags = mojom::kHitTestMine;
  c_hit_test_region_list->bounds.SetRect(0, 0, 200, 500);

  EXPECT_EQ(hit_test_manager()->GetRegionCount(), 0);

  // Submit in unexpected order.

  hit_test_manager()->SubmitHitTestRegionList(
      c_surface_id, 0, std::move(c_hit_test_region_list));
  hit_test_manager()->SubmitHitTestRegionList(
      e_surface_id, 0, std::move(e_hit_test_region_list));

  aggregator->Aggregate(e_surface_id);
  aggregator->SwapHandles();

  EXPECT_EQ(hit_test_manager()->GetRegionCount(), 2);

  // Discard Surface and ensure active count goes down.

  hit_test_manager()->CallOnSurfaceDiscarded(c_surface_id);
  EXPECT_EQ(hit_test_manager()->GetRegionCount(), 1);

  hit_test_manager()->CallOnSurfaceDiscarded(e_surface_id);
  EXPECT_EQ(hit_test_manager()->GetRegionCount(), 0);
}

}  // namespace viz
