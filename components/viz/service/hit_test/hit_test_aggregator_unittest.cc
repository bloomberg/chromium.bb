// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/hit_test/hit_test_aggregator.h"

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/host/hit_test/hit_test_query.h"
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

class TestHitTestAggregator : public HitTestAggregator {
 public:
  TestHitTestAggregator() = default;
  ~TestHitTestAggregator() = default;

  void CallOnSurfaceWillDraw(SurfaceId surface_id) {
    OnSurfaceWillDraw(surface_id);
  }
  void CallOnSurfaceDiscarded(SurfaceId surface_id) {
    OnSurfaceDiscarded(surface_id);
  }

  int Count() {
    AggregatedHitTestRegion* start =
        static_cast<AggregatedHitTestRegion*>(read_buffer_.get());
    AggregatedHitTestRegion* end = start;
    while (end->child_count != kEndOfList)
      end++;

    int count = end - start;
    return count;
  }
  int GetPendingCount() { return pending_.size(); }
  int GetActiveCount() { return active_.size(); }
  int GetActiveRegionCount() { return active_region_count_; }
  int GetHitTestRegionListSize() { return read_size_; }
  AggregatedHitTestRegion* GetRegions() {
    return static_cast<AggregatedHitTestRegion*>(read_buffer_.get());
  }

  void Reset() {
    AggregatedHitTestRegion* regions =
        static_cast<AggregatedHitTestRegion*>(write_buffer_.get());
    regions[0].child_count = kEndOfList;

    regions = static_cast<AggregatedHitTestRegion*>(read_buffer_.get());
    regions[0].child_count = kEndOfList;

    pending_.clear();
    active_.clear();
  }
};

class HitTestAggregatorTest : public testing::Test {
 public:
  HitTestAggregatorTest() = default;
  ~HitTestAggregatorTest() override = default;

  // testing::Test:
  void SetUp() override {}
  void TearDown() override { aggregator_.Reset(); }

  // Creates a hit test data element with 8 children recursively to
  // the specified depth.  SurfaceIds are generated in sequential order and
  // the method returns the next unused id.
  int CreateAndSubmitHitTestRegionListWith8Children(int id, int depth) {
    SurfaceId surface_id = MakeSurfaceId(kDisplayFrameSink, id);
    id++;

    auto hit_test_region_list = mojom::HitTestRegionList::New();
    hit_test_region_list->surface_id = surface_id;
    hit_test_region_list->flags = mojom::kHitTestMine;
    hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

    for (int i = 0; i < 8; i++) {
      auto hit_test_region = mojom::HitTestRegion::New();
      hit_test_region->rect.SetRect(100, 100, 100, 100);

      if (depth > 0) {
        hit_test_region->flags = mojom::kHitTestChildSurface;
        hit_test_region->surface_id = MakeSurfaceId(kDisplayFrameSink, id);
        id = CreateAndSubmitHitTestRegionListWith8Children(id, depth - 1);
      } else {
        hit_test_region->flags = mojom::kHitTestMine;
      }
      hit_test_region_list->regions.push_back(std::move(hit_test_region));
    }

    aggregator_.SubmitHitTestRegionList(std::move(hit_test_region_list));
    return id;
  }

  TestHitTestAggregator aggregator_;
  HitTestQuery hit_test_query_;
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
  EXPECT_EQ(0, aggregator_.Count());

  SurfaceId display_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);

  auto hit_test_region_list = mojom::HitTestRegionList::New();
  hit_test_region_list->surface_id = display_surface_id;
  hit_test_region_list->flags = mojom::kHitTestMine;
  hit_test_region_list->bounds.SetRect(0, 0, 1024, 768);

  aggregator_.SubmitHitTestRegionList(std::move(hit_test_region_list));
  EXPECT_EQ(0, aggregator_.Count());

  EXPECT_EQ(1, aggregator_.GetPendingCount());
  EXPECT_EQ(0, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(display_surface_id);

  EXPECT_EQ(0, aggregator_.GetPendingCount());
  EXPECT_EQ(1, aggregator_.GetActiveCount());

  aggregator_.Aggregate(display_surface_id);
  aggregator_.Swap();

  // Expect 1 entry routing all events to the one surface (display root).
  EXPECT_EQ(1, aggregator_.Count());

  AggregatedHitTestRegion* regions = aggregator_.GetRegions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(display_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(0, 0, 1024, 768), region->rect);
  EXPECT_EQ(0, region->child_count);

  hit_test_query_.set_aggregated_hit_test_region_list(regions, 1);

  // All points are in e's coordinate system when we reach this case.
  gfx::Point point1(1, 1);
  gfx::Point point2(1024, 768);
  gfx::Point point3(0, 0);

  Target target1 = hit_test_query_.FindTargetForLocation(point1);
  EXPECT_EQ(display_surface_id.frame_sink_id(), target1.frame_sink_id);
  EXPECT_EQ(point1, target1.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target1.flags);

  // point2 is on the bounds of e so no target found.
  Target target2 = hit_test_query_.FindTargetForLocation(point2);
  EXPECT_EQ(FrameSinkId(), target2.frame_sink_id);
  EXPECT_EQ(gfx::Point(), target2.location_in_target);
  EXPECT_FALSE(target2.flags);

  // There's a valid Target for point3, see Rect::Contains.
  Target target3 = hit_test_query_.FindTargetForLocation(point3);
  EXPECT_EQ(display_surface_id.frame_sink_id(), target3.frame_sink_id);
  EXPECT_EQ(point3, target3.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target3.flags);
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
  EXPECT_EQ(0, aggregator_.Count());

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);

  auto e_hit_test_data = mojom::HitTestRegionList::New();
  e_hit_test_data->surface_id = e_surface_id;
  e_hit_test_data->flags = mojom::kHitTestMine;
  e_hit_test_data->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_r1 = mojom::HitTestRegion::New();
  e_hit_test_region_r1->surface_id = e_surface_id;
  e_hit_test_region_r1->flags = mojom::kHitTestMine;
  e_hit_test_region_r1->rect.SetRect(100, 100, 200, 400);

  auto e_hit_test_region_r2 = mojom::HitTestRegion::New();
  e_hit_test_region_r2->surface_id = e_surface_id;
  e_hit_test_region_r2->flags = mojom::kHitTestMine;
  e_hit_test_region_r2->rect.SetRect(400, 100, 300, 400);

  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_r1));
  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_r2));

  // Submit mojom::HitTestRegionList.

  EXPECT_EQ(0, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(e_hit_test_data));
  EXPECT_EQ(1, aggregator_.GetPendingCount());

  // Add Surfaces to DisplayFrame in unexpected order.

  EXPECT_EQ(0, aggregator_.Count());
  EXPECT_EQ(0, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(e_surface_id);
  EXPECT_EQ(1, aggregator_.GetActiveCount());

  // Aggregate and swap.

  aggregator_.Aggregate(e_surface_id);
  EXPECT_EQ(0, aggregator_.Count());

  aggregator_.Swap();
  EXPECT_EQ(3, aggregator_.Count());

  AggregatedHitTestRegion* regions = aggregator_.GetRegions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(e_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(0, 0, 1024, 768), region->rect);
  EXPECT_EQ(2, region->child_count);

  region = &regions[1];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(gfx::Rect(100, 100, 200, 400), region->rect);
  EXPECT_EQ(0, region->child_count);

  region = &regions[2];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(gfx::Rect(400, 100, 300, 400), region->rect);
  EXPECT_EQ(0, region->child_count);

  hit_test_query_.set_aggregated_hit_test_region_list(regions, 3);

  // All points are in e's coordinate system when we reach this case.
  gfx::Point point1(99, 200);
  gfx::Point point2(150, 150);
  gfx::Point point3(400, 400);
  gfx::Point point4(1200, 350);

  Target target1 = hit_test_query_.FindTargetForLocation(point1);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target1.frame_sink_id);
  EXPECT_EQ(point1, target1.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target1.flags);

  Target target2 = hit_test_query_.FindTargetForLocation(point2);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target2.frame_sink_id);
  EXPECT_EQ(gfx::Point(50, 50), target2.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target2.flags);

  Target target3 = hit_test_query_.FindTargetForLocation(point3);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target3.frame_sink_id);
  EXPECT_EQ(gfx::Point(0, 300), target3.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target3.flags);

  Target target4 = hit_test_query_.FindTargetForLocation(point4);
  EXPECT_EQ(FrameSinkId(), target4.frame_sink_id);
  EXPECT_EQ(gfx::Point(), target4.location_in_target);
  EXPECT_FALSE(target4.flags);
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
  EXPECT_EQ(0, aggregator_.Count());

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c1_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);
  SurfaceId c2_surface_id = MakeSurfaceId(kDisplayFrameSink, 3);

  auto e_hit_test_data = mojom::HitTestRegionList::New();
  e_hit_test_data->surface_id = e_surface_id;
  e_hit_test_data->flags = mojom::kHitTestMine;
  e_hit_test_data->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_c1 = mojom::HitTestRegion::New();
  e_hit_test_region_c1->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c1->surface_id = c1_surface_id;
  e_hit_test_region_c1->rect.SetRect(100, 100, 200, 300);

  auto e_hit_test_region_c2 = mojom::HitTestRegion::New();
  e_hit_test_region_c2->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c2->surface_id = c2_surface_id;
  e_hit_test_region_c2->rect.SetRect(400, 100, 400, 300);

  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_c1));
  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_c2));

  auto c1_hit_test_data = mojom::HitTestRegionList::New();
  c1_hit_test_data->surface_id = c1_surface_id;

  auto c2_hit_test_data = mojom::HitTestRegionList::New();
  c2_hit_test_data->surface_id = c2_surface_id;

  // Submit in unexpected order.

  EXPECT_EQ(0, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(c1_hit_test_data));
  EXPECT_EQ(1, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(e_hit_test_data));
  EXPECT_EQ(2, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(c2_hit_test_data));
  EXPECT_EQ(3, aggregator_.GetPendingCount());

  // Surfaces added to DisplayFrame in unexpected order.

  EXPECT_EQ(0, aggregator_.Count());

  EXPECT_EQ(0, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(c2_surface_id);
  EXPECT_EQ(1, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(c1_surface_id);
  EXPECT_EQ(2, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(e_surface_id);
  EXPECT_EQ(3, aggregator_.GetActiveCount());

  // Aggregate and swap.

  aggregator_.Aggregate(e_surface_id);
  EXPECT_EQ(0, aggregator_.Count());

  aggregator_.Swap();

  EXPECT_EQ(3, aggregator_.Count());

  AggregatedHitTestRegion* regions = aggregator_.GetRegions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(e_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(0, 0, 1024, 768), region->rect);
  EXPECT_EQ(2, region->child_count);

  region = &regions[1];
  EXPECT_EQ(mojom::kHitTestChildSurface, region->flags);
  EXPECT_EQ(c1_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(100, 100, 200, 300), region->rect);
  EXPECT_EQ(0, region->child_count);

  region = &regions[2];
  EXPECT_EQ(mojom::kHitTestChildSurface, region->flags);
  EXPECT_EQ(c2_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(400, 100, 400, 300), region->rect);
  EXPECT_EQ(0, region->child_count);

  hit_test_query_.set_aggregated_hit_test_region_list(regions, 3);

  // All points are in e's coordinate system when we reach this case.
  // They all go to e since c1 and c2 cannot receive events.
  gfx::Point point1(99, 200);
  gfx::Point point2(150, 150);
  gfx::Point point3(400, 400);
  gfx::Point point4(1200, 350);

  Target target1 = hit_test_query_.FindTargetForLocation(point1);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target1.frame_sink_id);
  EXPECT_EQ(point1, target1.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target1.flags);

  Target target2 = hit_test_query_.FindTargetForLocation(point2);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target2.frame_sink_id);
  EXPECT_EQ(point2, target2.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target2.flags);

  Target target3 = hit_test_query_.FindTargetForLocation(point3);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target3.frame_sink_id);
  EXPECT_EQ(point3, target3.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target3.flags);

  Target target4 = hit_test_query_.FindTargetForLocation(point4);
  EXPECT_EQ(FrameSinkId(), target4.frame_sink_id);
  EXPECT_EQ(gfx::Point(), target4.location_in_target);
  EXPECT_FALSE(target4.flags);
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
  EXPECT_EQ(0, aggregator_.Count());

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);

  auto e_hit_test_data = mojom::HitTestRegionList::New();
  e_hit_test_data->surface_id = e_surface_id;
  e_hit_test_data->flags = mojom::kHitTestMine;
  e_hit_test_data->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_div = mojom::HitTestRegion::New();
  e_hit_test_region_div->flags = mojom::kHitTestMine;
  e_hit_test_region_div->surface_id = e_surface_id;
  e_hit_test_region_div->rect.SetRect(200, 200, 300, 200);

  auto e_hit_test_region_c = mojom::HitTestRegion::New();
  e_hit_test_region_c->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c->surface_id = c_surface_id;
  e_hit_test_region_c->rect.SetRect(100, 100, 200, 500);

  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_div));
  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_c));

  auto c_hit_test_data = mojom::HitTestRegionList::New();
  c_hit_test_data->surface_id = c_surface_id;
  c_hit_test_data->flags = mojom::kHitTestMine;
  c_hit_test_data->bounds.SetRect(0, 0, 200, 500);

  // Submit in unexpected order.

  EXPECT_EQ(0, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(c_hit_test_data));
  EXPECT_EQ(1, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(e_hit_test_data));
  EXPECT_EQ(2, aggregator_.GetPendingCount());

  // Surfaces added to DisplayFrame in unexpected order.

  EXPECT_EQ(0, aggregator_.Count());

  EXPECT_EQ(0, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(e_surface_id);
  EXPECT_EQ(1, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(c_surface_id);
  EXPECT_EQ(2, aggregator_.GetActiveCount());

  // Aggregate and swap.

  aggregator_.Aggregate(e_surface_id);
  EXPECT_EQ(0, aggregator_.Count());

  aggregator_.Swap();

  EXPECT_EQ(3, aggregator_.Count());

  AggregatedHitTestRegion* regions = aggregator_.GetRegions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(e_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(0, 0, 1024, 768), region->rect);
  EXPECT_EQ(2, region->child_count);

  region = &regions[1];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(e_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(200, 200, 300, 200), region->rect);
  EXPECT_EQ(0, region->child_count);

  region = &regions[2];
  EXPECT_EQ(region->flags, mojom::kHitTestChildSurface | mojom::kHitTestMine);
  EXPECT_EQ(c_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(100, 100, 200, 500), region->rect);
  EXPECT_EQ(0, region->child_count);

  hit_test_query_.set_aggregated_hit_test_region_list(regions, 3);

  // All points are in e's coordinate system when we reach this case.
  gfx::Point point1(99, 200);
  gfx::Point point2(150, 150);
  gfx::Point point3(250, 250);

  Target target1 = hit_test_query_.FindTargetForLocation(point1);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target1.frame_sink_id);
  EXPECT_EQ(point1, target1.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target1.flags);

  Target target2 = hit_test_query_.FindTargetForLocation(point2);
  EXPECT_EQ(c_surface_id.frame_sink_id(), target2.frame_sink_id);
  EXPECT_EQ(gfx::Point(50, 50), target2.location_in_target);
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, target2.flags);

  Target target3 = hit_test_query_.FindTargetForLocation(point3);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target3.frame_sink_id);
  EXPECT_EQ(gfx::Point(50, 50), target3.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target3.flags);
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
  EXPECT_EQ(0, aggregator_.Count());

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);

  auto e_hit_test_data = mojom::HitTestRegionList::New();
  e_hit_test_data->surface_id = e_surface_id;
  e_hit_test_data->flags = mojom::kHitTestMine;
  e_hit_test_data->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_div = mojom::HitTestRegion::New();
  e_hit_test_region_div->flags = mojom::kHitTestMine;
  e_hit_test_region_div->surface_id = e_surface_id;
  e_hit_test_region_div->rect.SetRect(200, 200, 300, 200);

  auto e_hit_test_region_c = mojom::HitTestRegion::New();
  e_hit_test_region_c->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c->surface_id = c_surface_id;
  e_hit_test_region_c->rect.SetRect(100, 100, 200, 500);

  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_c));
  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_div));

  auto c_hit_test_data = mojom::HitTestRegionList::New();
  c_hit_test_data->surface_id = c_surface_id;
  c_hit_test_data->flags = mojom::kHitTestMine;
  c_hit_test_data->bounds.SetRect(0, 0, 200, 500);

  // Submit in unexpected order.

  EXPECT_EQ(0, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(c_hit_test_data));
  EXPECT_EQ(1, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(e_hit_test_data));
  EXPECT_EQ(2, aggregator_.GetPendingCount());

  // Surfaces added to DisplayFrame in unexpected order.

  EXPECT_EQ(0, aggregator_.Count());

  EXPECT_EQ(0, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(e_surface_id);
  EXPECT_EQ(1, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(c_surface_id);
  EXPECT_EQ(2, aggregator_.GetActiveCount());

  // Aggregate and swap.

  aggregator_.Aggregate(e_surface_id);
  EXPECT_EQ(0, aggregator_.Count());

  aggregator_.Swap();

  EXPECT_EQ(3, aggregator_.Count());

  AggregatedHitTestRegion* regions = aggregator_.GetRegions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(e_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(0, 0, 1024, 768), region->rect);
  EXPECT_EQ(2, region->child_count);

  region = &regions[1];
  EXPECT_EQ(region->flags, mojom::kHitTestChildSurface | mojom::kHitTestMine);
  EXPECT_EQ(c_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(100, 100, 200, 500), region->rect);
  EXPECT_EQ(0, region->child_count);

  region = &regions[2];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(e_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(200, 200, 300, 200), region->rect);
  EXPECT_EQ(0, region->child_count);

  hit_test_query_.set_aggregated_hit_test_region_list(regions, 3);

  // All points are in e's coordinate system when we reach this case.
  gfx::Point point1(99, 200);
  gfx::Point point2(150, 150);
  gfx::Point point3(250, 250);
  gfx::Point point4(350, 300);

  Target target1 = hit_test_query_.FindTargetForLocation(point1);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target1.frame_sink_id);
  EXPECT_EQ(point1, target1.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target1.flags);

  Target target2 = hit_test_query_.FindTargetForLocation(point2);
  EXPECT_EQ(c_surface_id.frame_sink_id(), target2.frame_sink_id);
  EXPECT_EQ(gfx::Point(50, 50), target2.location_in_target);
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, target2.flags);

  Target target3 = hit_test_query_.FindTargetForLocation(point3);
  EXPECT_EQ(c_surface_id.frame_sink_id(), target3.frame_sink_id);
  EXPECT_EQ(gfx::Point(150, 150), target3.location_in_target);
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, target3.flags);

  Target target4 = hit_test_query_.FindTargetForLocation(point4);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target4.frame_sink_id);
  EXPECT_EQ(gfx::Point(150, 100), target4.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target4.flags);
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
  EXPECT_EQ(0, aggregator_.Count());

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);
  SurfaceId a_surface_id = MakeSurfaceId(kDisplayFrameSink, 3);
  SurfaceId b_surface_id = MakeSurfaceId(kDisplayFrameSink, 4);

  auto e_hit_test_data = mojom::HitTestRegionList::New();
  e_hit_test_data->surface_id = e_surface_id;
  e_hit_test_data->flags = mojom::kHitTestMine;
  e_hit_test_data->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_c = mojom::HitTestRegion::New();
  e_hit_test_region_c->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c->surface_id = c_surface_id;
  e_hit_test_region_c->rect.SetRect(300, 100, 1600, 800);
  e_hit_test_region_c->transform.Translate(200, 100);

  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_c));

  auto c_hit_test_data = mojom::HitTestRegionList::New();
  c_hit_test_data->surface_id = c_surface_id;
  c_hit_test_data->flags = mojom::kHitTestIgnore;
  c_hit_test_data->bounds.SetRect(0, 0, 1600, 800);

  auto c_hit_test_region_a = mojom::HitTestRegion::New();
  c_hit_test_region_a->flags = mojom::kHitTestChildSurface;
  c_hit_test_region_a->surface_id = a_surface_id;
  c_hit_test_region_a->rect.SetRect(0, 0, 200, 100);

  auto c_hit_test_region_b = mojom::HitTestRegion::New();
  c_hit_test_region_b->flags = mojom::kHitTestChildSurface;
  c_hit_test_region_b->surface_id = b_surface_id;
  c_hit_test_region_b->rect.SetRect(0, 100, 800, 600);

  c_hit_test_data->regions.push_back(std::move(c_hit_test_region_a));
  c_hit_test_data->regions.push_back(std::move(c_hit_test_region_b));

  auto a_hit_test_data = mojom::HitTestRegionList::New();
  a_hit_test_data->surface_id = a_surface_id;
  a_hit_test_data->flags = mojom::kHitTestMine;
  a_hit_test_data->bounds.SetRect(0, 0, 200, 100);

  auto b_hit_test_data = mojom::HitTestRegionList::New();
  b_hit_test_data->surface_id = b_surface_id;
  b_hit_test_data->flags = mojom::kHitTestMine;
  b_hit_test_data->bounds.SetRect(0, 100, 800, 600);

  // Submit in unexpected order.

  EXPECT_EQ(0, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(c_hit_test_data));
  EXPECT_EQ(1, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(a_hit_test_data));
  EXPECT_EQ(2, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(b_hit_test_data));
  EXPECT_EQ(3, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(e_hit_test_data));
  EXPECT_EQ(4, aggregator_.GetPendingCount());

  // Surfaces added to DisplayFrame in unexpected order.

  EXPECT_EQ(0, aggregator_.Count());

  EXPECT_EQ(0, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(c_surface_id);
  EXPECT_EQ(1, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(e_surface_id);
  EXPECT_EQ(2, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(b_surface_id);
  EXPECT_EQ(3, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(a_surface_id);
  EXPECT_EQ(4, aggregator_.GetActiveCount());

  // Aggregate and swap.

  aggregator_.Aggregate(e_surface_id);
  EXPECT_EQ(0, aggregator_.Count());

  aggregator_.Swap();

  EXPECT_EQ(4, aggregator_.Count());

  AggregatedHitTestRegion* regions = aggregator_.GetRegions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(e_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(0, 0, 1024, 768), region->rect);
  EXPECT_EQ(3, region->child_count);

  region = &regions[1];
  EXPECT_EQ(region->flags, mojom::kHitTestChildSurface | mojom::kHitTestIgnore);
  EXPECT_EQ(c_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(300, 100, 1600, 800), region->rect);
  EXPECT_EQ(2, region->child_count);

  gfx::Point point(300, 300);
  region->transform.TransformPointReverse(&point);
  EXPECT_TRUE(point == gfx::Point(100, 200));

  region = &regions[2];
  EXPECT_EQ(region->flags, mojom::kHitTestChildSurface | mojom::kHitTestMine);
  EXPECT_EQ(a_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(0, 0, 200, 100), region->rect);
  EXPECT_EQ(0, region->child_count);

  region = &regions[3];
  EXPECT_EQ(region->flags, mojom::kHitTestChildSurface | mojom::kHitTestMine);
  EXPECT_EQ(b_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(0, 100, 800, 600), region->rect);
  EXPECT_EQ(0, region->child_count);

  hit_test_query_.set_aggregated_hit_test_region_list(regions, 4);

  // All points are in e's coordinate system when we reach this case.
  gfx::Point point1(1, 1);
  gfx::Point point2(100, 50);
  gfx::Point point3(400, 70);
  gfx::Point point4(200, 200);

  Target target1 = hit_test_query_.FindTargetForLocation(point1);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target1.frame_sink_id);
  EXPECT_EQ(point1, target1.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target1.flags);

  Target target2 = hit_test_query_.FindTargetForLocation(point2);
  EXPECT_EQ(a_surface_id.frame_sink_id(), target2.frame_sink_id);
  EXPECT_EQ(gfx::Point(0, 50), target2.location_in_target);
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, target2.flags);

  Target target3 = hit_test_query_.FindTargetForLocation(point3);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target3.frame_sink_id);
  EXPECT_EQ(point3, target3.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target3.flags);

  Target target4 = hit_test_query_.FindTargetForLocation(point4);
  EXPECT_EQ(b_surface_id.frame_sink_id(), target4.frame_sink_id);
  EXPECT_EQ(gfx::Point(100, 100), target4.location_in_target);
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, target4.flags);
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
  EXPECT_EQ(0, aggregator_.Count());

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c1_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);
  SurfaceId c2_surface_id = MakeSurfaceId(kDisplayFrameSink, 3);
  SurfaceId c3_surface_id = MakeSurfaceId(kDisplayFrameSink, 4);

  auto e_hit_test_data = mojom::HitTestRegionList::New();
  e_hit_test_data->surface_id = e_surface_id;
  e_hit_test_data->flags = mojom::kHitTestMine;
  e_hit_test_data->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_c1 = mojom::HitTestRegion::New();
  e_hit_test_region_c1->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c1->surface_id = c1_surface_id;
  e_hit_test_region_c1->rect.SetRect(100, 100, 700, 700);

  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_c1));

  auto c1_hit_test_data = mojom::HitTestRegionList::New();
  c1_hit_test_data->surface_id = c1_surface_id;
  c1_hit_test_data->flags = mojom::kHitTestMine;
  c1_hit_test_data->bounds.SetRect(0, 0, 600, 600);

  auto c1_hit_test_region_c2 = mojom::HitTestRegion::New();
  c1_hit_test_region_c2->flags = mojom::kHitTestChildSurface;
  c1_hit_test_region_c2->surface_id = c2_surface_id;
  c1_hit_test_region_c2->rect.SetRect(100, 100, 500, 500);

  c1_hit_test_data->regions.push_back(std::move(c1_hit_test_region_c2));

  auto c2_hit_test_data = mojom::HitTestRegionList::New();
  c2_hit_test_data->surface_id = c2_surface_id;
  c2_hit_test_data->flags = mojom::kHitTestMine;
  c2_hit_test_data->bounds.SetRect(0, 0, 400, 400);

  auto c2_hit_test_region_c3 = mojom::HitTestRegion::New();
  c2_hit_test_region_c3->flags = mojom::kHitTestChildSurface;
  c2_hit_test_region_c3->surface_id = c3_surface_id;
  c2_hit_test_region_c3->rect.SetRect(100, 100, 300, 300);

  c2_hit_test_data->regions.push_back(std::move(c2_hit_test_region_c3));

  auto c3_hit_test_data = mojom::HitTestRegionList::New();
  c3_hit_test_data->surface_id = c3_surface_id;
  c3_hit_test_data->flags = mojom::kHitTestMine;
  c3_hit_test_data->bounds.SetRect(0, 0, 200, 200);

  // Submit in unexpected order.

  EXPECT_EQ(0, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(c1_hit_test_data));
  EXPECT_EQ(1, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(c3_hit_test_data));
  EXPECT_EQ(2, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(e_hit_test_data));
  EXPECT_EQ(3, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(c2_hit_test_data));
  EXPECT_EQ(4, aggregator_.GetPendingCount());

  // Surfaces added to DisplayFrame in unexpected order.

  EXPECT_EQ(0, aggregator_.Count());

  EXPECT_EQ(0, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(c2_surface_id);
  EXPECT_EQ(1, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(c1_surface_id);
  EXPECT_EQ(2, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(e_surface_id);
  EXPECT_EQ(3, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(c3_surface_id);
  EXPECT_EQ(4, aggregator_.GetActiveCount());

  // Aggregate and swap.

  aggregator_.Aggregate(e_surface_id);
  EXPECT_EQ(0, aggregator_.Count());

  aggregator_.Swap();

  EXPECT_EQ(4, aggregator_.Count());

  AggregatedHitTestRegion* regions = aggregator_.GetRegions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(e_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(0, 0, 1024, 768), region->rect);
  EXPECT_EQ(3, region->child_count);

  region = &regions[1];
  EXPECT_EQ(region->flags, mojom::kHitTestChildSurface | mojom::kHitTestMine);
  EXPECT_EQ(c1_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(100, 100, 700, 700), region->rect);
  EXPECT_EQ(2, region->child_count);

  region = &regions[2];
  EXPECT_EQ(region->flags, mojom::kHitTestChildSurface | mojom::kHitTestMine);
  EXPECT_EQ(c2_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(100, 100, 500, 500), region->rect);
  EXPECT_EQ(1, region->child_count);

  region = &regions[3];
  EXPECT_EQ(region->flags, mojom::kHitTestChildSurface | mojom::kHitTestMine);
  EXPECT_EQ(c3_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(100, 100, 300, 300), region->rect);
  EXPECT_EQ(0, region->child_count);

  hit_test_query_.set_aggregated_hit_test_region_list(regions, 4);

  // All points are in e's coordinate system when we reach this case.
  gfx::Point point1(1, 1);
  gfx::Point point2(100, 150);
  gfx::Point point3(250, 450);
  gfx::Point point4(450, 550);

  Target target1 = hit_test_query_.FindTargetForLocation(point1);
  EXPECT_EQ(e_surface_id.frame_sink_id(), target1.frame_sink_id);
  EXPECT_EQ(point1, target1.location_in_target);
  EXPECT_EQ(mojom::kHitTestMine, target1.flags);

  Target target2 = hit_test_query_.FindTargetForLocation(point2);
  EXPECT_EQ(c3_surface_id.frame_sink_id(), target2.frame_sink_id);
  EXPECT_EQ(gfx::Point(0, 50), target2.location_in_target);
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, target2.flags);

  Target target3 = hit_test_query_.FindTargetForLocation(point3);
  EXPECT_EQ(c2_surface_id.frame_sink_id(), target3.frame_sink_id);
  EXPECT_EQ(gfx::Point(50, 250), target3.location_in_target);
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, target3.flags);

  Target target4 = hit_test_query_.FindTargetForLocation(point4);
  EXPECT_EQ(c1_surface_id.frame_sink_id(), target4.frame_sink_id);
  EXPECT_EQ(gfx::Point(150, 250), target4.location_in_target);
  EXPECT_EQ(mojom::kHitTestChildSurface | mojom::kHitTestMine, target4.flags);
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
  EXPECT_EQ(0, aggregator_.Count());

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);

  auto e_hit_test_data = mojom::HitTestRegionList::New();
  e_hit_test_data->surface_id = e_surface_id;
  e_hit_test_data->flags = mojom::kHitTestMine;
  e_hit_test_data->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_div = mojom::HitTestRegion::New();
  e_hit_test_region_div->flags = mojom::kHitTestMine;
  e_hit_test_region_div->surface_id = e_surface_id;
  e_hit_test_region_div->rect.SetRect(200, 200, 300, 200);

  auto e_hit_test_region_c = mojom::HitTestRegion::New();
  e_hit_test_region_c->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c->surface_id = c_surface_id;
  e_hit_test_region_c->rect.SetRect(100, 100, 200, 500);

  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_c));
  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_div));

  auto c_hit_test_data = mojom::HitTestRegionList::New();
  c_hit_test_data->surface_id = c_surface_id;
  c_hit_test_data->flags = mojom::kHitTestMine;
  c_hit_test_data->bounds.SetRect(0, 0, 200, 500);

  // Submit in unexpected order, but not the child.

  EXPECT_EQ(0, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(e_hit_test_data));
  EXPECT_EQ(1, aggregator_.GetPendingCount());

  // Surfaces added to DisplayFrame in unexpected order.

  EXPECT_EQ(0, aggregator_.Count());

  EXPECT_EQ(0, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(e_surface_id);
  EXPECT_EQ(1, aggregator_.GetActiveCount());

  // Aggregate and swap.

  aggregator_.Aggregate(e_surface_id);
  EXPECT_EQ(0, aggregator_.Count());

  aggregator_.Swap();

  EXPECT_EQ(2, aggregator_.Count());

  AggregatedHitTestRegion* regions = aggregator_.GetRegions();

  AggregatedHitTestRegion* region = nullptr;

  region = &regions[0];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(e_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(0, 0, 1024, 768), region->rect);
  EXPECT_EQ(1, region->child_count);

  // Child would exist here but it was not included in the Display Frame.

  region = &regions[1];
  EXPECT_EQ(mojom::kHitTestMine, region->flags);
  EXPECT_EQ(e_surface_id.frame_sink_id(), region->frame_sink_id);
  EXPECT_EQ(gfx::Rect(200, 200, 300, 200), region->rect);
  EXPECT_EQ(0, region->child_count);
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
  EXPECT_EQ(0, aggregator_.Count());

  EXPECT_LT(aggregator_.GetHitTestRegionListSize(), 4096);

  SurfaceId display_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);

  int next_surface_id = CreateAndSubmitHitTestRegionListWith8Children(1, 3);
  int surface_count = next_surface_id - 1;

  EXPECT_EQ(surface_count, aggregator_.GetPendingCount());

  // Mark Surfaces as added to DisplayFrame in unexpected order.

  EXPECT_EQ(0, aggregator_.Count());
  EXPECT_EQ(0, aggregator_.GetActiveCount());

  for (int i = 1; i <= surface_count; i++) {
    SurfaceId surface_id = MakeSurfaceId(kDisplayFrameSink, i);
    aggregator_.CallOnSurfaceWillDraw(surface_id);
  }

  EXPECT_EQ(surface_count, aggregator_.GetActiveCount());

  // Aggregate and swap.
  aggregator_.Aggregate(display_surface_id);
  EXPECT_EQ(0, aggregator_.Count());

  aggregator_.Swap();

  // Expect 4680 regions:
  //  8 children 4 levels deep 8*8*8*8 is  4096
  //  1 region for each embedder/surface +  584
  //  1 root                             +    1
  //                                      -----
  //                                       4681.
  EXPECT_EQ(4681, aggregator_.Count());

  EXPECT_GE(aggregator_.GetHitTestRegionListSize(), 4681);
}

TEST_F(HitTestAggregatorTest, ActiveRegionCount) {
  EXPECT_EQ(0, aggregator_.GetActiveRegionCount());

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayFrameSink, 1);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayFrameSink, 2);

  auto e_hit_test_data = mojom::HitTestRegionList::New();
  e_hit_test_data->surface_id = e_surface_id;
  e_hit_test_data->flags = mojom::kHitTestMine;
  e_hit_test_data->bounds.SetRect(0, 0, 1024, 768);

  auto e_hit_test_region_div = mojom::HitTestRegion::New();
  e_hit_test_region_div->flags = mojom::kHitTestMine;
  e_hit_test_region_div->surface_id = e_surface_id;
  e_hit_test_region_div->rect.SetRect(200, 200, 300, 200);

  auto e_hit_test_region_c = mojom::HitTestRegion::New();
  e_hit_test_region_c->flags = mojom::kHitTestChildSurface;
  e_hit_test_region_c->surface_id = c_surface_id;
  e_hit_test_region_c->rect.SetRect(100, 100, 200, 500);

  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_c));
  e_hit_test_data->regions.push_back(std::move(e_hit_test_region_div));

  auto c_hit_test_data = mojom::HitTestRegionList::New();
  c_hit_test_data->surface_id = c_surface_id;
  c_hit_test_data->flags = mojom::kHitTestMine;
  c_hit_test_data->bounds.SetRect(0, 0, 200, 500);

  EXPECT_EQ(0, aggregator_.GetActiveRegionCount());

  // Submit in unexpected order.

  EXPECT_EQ(0, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(c_hit_test_data));
  EXPECT_EQ(1, aggregator_.GetPendingCount());

  aggregator_.SubmitHitTestRegionList(std::move(e_hit_test_data));
  EXPECT_EQ(2, aggregator_.GetPendingCount());

  EXPECT_EQ(0, aggregator_.GetActiveRegionCount());

  // Surfaces added to DisplayFrame in unexpected order.

  EXPECT_EQ(0, aggregator_.Count());
  EXPECT_EQ(0, aggregator_.GetActiveCount());

  aggregator_.CallOnSurfaceWillDraw(e_surface_id);
  EXPECT_EQ(1, aggregator_.GetActiveCount());
  EXPECT_EQ(2, aggregator_.GetActiveRegionCount());

  aggregator_.CallOnSurfaceWillDraw(c_surface_id);
  EXPECT_EQ(2, aggregator_.GetActiveCount());
  EXPECT_EQ(2, aggregator_.GetActiveRegionCount());

  // Aggregate and swap.

  aggregator_.Aggregate(e_surface_id);
  EXPECT_EQ(0, aggregator_.Count());
  EXPECT_EQ(2, aggregator_.GetActiveRegionCount());

  aggregator_.Swap();

  EXPECT_EQ(3, aggregator_.Count());
  EXPECT_EQ(2, aggregator_.GetActiveRegionCount());

  // Discard Surface and ensure active count goes down.

  aggregator_.CallOnSurfaceDiscarded(c_surface_id);
  EXPECT_EQ(2, aggregator_.GetActiveRegionCount());

  aggregator_.CallOnSurfaceDiscarded(e_surface_id);
  EXPECT_EQ(0, aggregator_.GetActiveRegionCount());
}

}  // namespace viz
