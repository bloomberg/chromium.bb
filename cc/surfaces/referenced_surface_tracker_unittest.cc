// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/referenced_surface_tracker.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_reference.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;
using testing::IsEmpty;
using testing::SizeIs;

namespace cc {
namespace test {
namespace {

constexpr FrameSinkId kParentFrameSink(2, 1);
constexpr FrameSinkId kChildFrameSink1(65563, 1);
constexpr FrameSinkId kChildFrameSink2(65564, 1);

SurfaceId MakeSurfaceId(const FrameSinkId& frame_sink_id, uint32_t local_id) {
  return SurfaceId(
      frame_sink_id,
      LocalSurfaceId(local_id, base::UnguessableToken::Deserialize(0, 1u)));
}

}  // namespace

class ReferencedSurfaceTrackerTest : public testing::Test {
 public:
  ReferencedSurfaceTrackerTest() {}
  ~ReferencedSurfaceTrackerTest() override {}

  ReferencedSurfaceTracker& tracker() { return *tracker_; }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    tracker_ = base::MakeUnique<ReferencedSurfaceTracker>(kParentFrameSink);
  }

  void TearDown() override { tracker_.reset(); }

 private:
  std::unique_ptr<ReferencedSurfaceTracker> tracker_;

  DISALLOW_COPY_AND_ASSIGN(ReferencedSurfaceTrackerTest);
};

TEST_F(ReferencedSurfaceTrackerTest, SetCurrentSurfaceId) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);

  // Initially current_surface_id() should be invalid.
  EXPECT_FALSE(tracker().current_surface_id().is_valid());

  // After setting current SurfaceId then current_surface_id() should be valid.
  tracker().UpdateReferences(parent_id.local_surface_id(), nullptr, nullptr);
  EXPECT_EQ(parent_id, tracker().current_surface_id());
}

TEST_F(ReferencedSurfaceTrackerTest, RefSurface) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceReference reference(parent_id, child_id1);

  // First frame has a reference to |child_id1|, check that reference is added.
  std::vector<SurfaceId> active_referenced_surfaces = {child_id1};
  tracker().UpdateReferences(parent_id.local_surface_id(),
                             &active_referenced_surfaces,
                             nullptr /* pending_referenced_surfaces */);
  EXPECT_THAT(tracker().references_to_add(), UnorderedElementsAre(reference));
  EXPECT_THAT(tracker().references_to_remove(), IsEmpty());
}

TEST_F(ReferencedSurfaceTrackerTest, NoChangeToReferences) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceReference reference(parent_id, child_id1);

  // First frame has a reference to |child_id1|, check that reference is added.
  std::vector<SurfaceId> active_referenced_surfaces = {child_id1};
  tracker().UpdateReferences(parent_id.local_surface_id(),
                             &active_referenced_surfaces,
                             nullptr /* pending_referenced_surfaces */);
  EXPECT_THAT(tracker().references_to_remove(), IsEmpty());
  EXPECT_THAT(tracker().references_to_add(), UnorderedElementsAre(reference));

  // Second frame has same reference, check that no references are added or
  // removed.
  tracker().UpdateReferences(parent_id.local_surface_id(),
                             &active_referenced_surfaces,
                             nullptr /* pending_referenced_surfaces */);
  EXPECT_THAT(tracker().references_to_remove(), IsEmpty());
  EXPECT_THAT(tracker().references_to_add(), IsEmpty());
}

TEST_F(ReferencedSurfaceTrackerTest, UnrefSurface) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceReference reference(parent_id, child_id1);

  std::vector<SurfaceId> active_referenced_surfaces = {child_id1};
  tracker().UpdateReferences(parent_id.local_surface_id(),
                             &active_referenced_surfaces,
                             nullptr /* pending_referenced_surfaces */);

  // Second frame no longer references |child_id1|, check that reference to is
  // removed.
  tracker().UpdateReferences(parent_id.local_surface_id(),
                             nullptr /* active_referenced_surfaces */,
                             nullptr /* pending_referenced_surfaces */);
  EXPECT_THAT(tracker().references_to_add(), IsEmpty());
  EXPECT_THAT(tracker().references_to_remove(),
              UnorderedElementsAre(reference));
}

TEST_F(ReferencedSurfaceTrackerTest, RefNewSurfaceForFrameSink) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1_first = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id1_second = MakeSurfaceId(kChildFrameSink1, 2);
  const SurfaceReference reference_first(parent_id, child_id1_first);
  const SurfaceReference reference_second(parent_id, child_id1_second);

  // First frame has reference to |child_id1_first|.
  std::vector<SurfaceId> active_referenced_surfaces1 = {child_id1_first};
  tracker().UpdateReferences(parent_id.local_surface_id(),
                             &active_referenced_surfaces1,
                             nullptr /* pending_referenced_surfaces */);
  EXPECT_THAT(tracker().references_to_add(),
              UnorderedElementsAre(reference_first));

  // Second frame has reference to |child_id1_second| which has the same
  // FrameSinkId but different LocalSurfaceId. Check that first reference is
  // removed and second reference is added.
  std::vector<SurfaceId> active_referenced_surfaces2 = {child_id1_second};
  tracker().UpdateReferences(parent_id.local_surface_id(),
                             &active_referenced_surfaces2,
                             nullptr /* pending_referenced_surfaces */);
  EXPECT_THAT(tracker().references_to_remove(),
              UnorderedElementsAre(reference_first));
  EXPECT_THAT(tracker().references_to_add(),
              UnorderedElementsAre(reference_second));
}

TEST_F(ReferencedSurfaceTrackerTest, UpdateParentSurfaceId) {
  const SurfaceId parent_id_first = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id_second = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceReference reference(parent_id_second, child_id1);

  // First frame references |child_id1|.
  std::vector<SurfaceId> active_referenced_surfaces = {child_id1};
  tracker().UpdateReferences(parent_id_first.local_surface_id(),
                             &active_referenced_surfaces,
                             nullptr /* pending_referenced_surfaces */);
  EXPECT_THAT(tracker().references_to_add(), SizeIs(1));

  // Second frame still reference |child_id1| but the parent SurfaceId has
  // changed. The new parent SurfaceId should have a reference added to
  // |child_id1|.
  tracker().UpdateReferences(parent_id_second.local_surface_id(),
                             &active_referenced_surfaces,
                             nullptr /* pending_referenced_surfaces */);
  EXPECT_THAT(tracker().references_to_add(), UnorderedElementsAre(reference));
  EXPECT_THAT(tracker().references_to_remove(), IsEmpty());
}

TEST_F(ReferencedSurfaceTrackerTest, RefTwoThenUnrefOneSurface) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 2);
  const SurfaceReference reference1(parent_id, child_id1);
  const SurfaceReference reference2(parent_id, child_id2);

  // First frame references both surfaces.
  std::vector<SurfaceId> active_referenced_surfaces1 = {child_id1, child_id2};
  tracker().UpdateReferences(parent_id.local_surface_id(),
                             &active_referenced_surfaces1,
                             nullptr /* pending_referenced_surfaces */);
  EXPECT_THAT(tracker().references_to_add(),
              UnorderedElementsAre(reference1, reference2));

  // Second frame references only |child_id2|, check that reference to
  // |child_id1| is removed.
  std::vector<SurfaceId> active_referenced_surfaces2 = {child_id2};
  tracker().UpdateReferences(parent_id.local_surface_id(),
                             &active_referenced_surfaces2,
                             nullptr /* pending_referenced_surfaces */);
  EXPECT_THAT(tracker().references_to_remove(),
              UnorderedElementsAre(reference1));
  EXPECT_THAT(tracker().references_to_add(), IsEmpty());
}

}  // namespace test
}  // namespace cc
