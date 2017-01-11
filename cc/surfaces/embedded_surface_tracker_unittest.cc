// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/embedded_surface_tracker.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_reference.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;

namespace cc {
namespace test {
namespace {

constexpr FrameSinkId kParentFrameSink(2, 1);
constexpr FrameSinkId kEmbeddedFrameSink(65563, 1);

SurfaceId MakeSurfaceId(const FrameSinkId& frame_sink_id, uint32_t local_id) {
  return SurfaceId(
      frame_sink_id,
      LocalFrameId(local_id, base::UnguessableToken::Deserialize(0, 1u)));
}

}  // namespace

class EmbeddedSurfaceTrackerTest : public testing::Test {
 public:
  EmbeddedSurfaceTrackerTest() {}
  ~EmbeddedSurfaceTrackerTest() override {}

  EmbeddedSurfaceTracker& tracker() { return *tracker_; }

  // Returns references to add without clearing them.
  const std::vector<SurfaceReference>& peek_to_add() {
    return tracker_->references_to_add_;
  }

  // Returns references to remove without clearing them.
  const std::vector<SurfaceReference>& peek_to_remove() {
    return tracker_->references_to_remove_;
  }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    tracker_ = base::MakeUnique<EmbeddedSurfaceTracker>();
  }

  void TearDown() override { tracker_.reset(); }

 private:
  std::unique_ptr<EmbeddedSurfaceTracker> tracker_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedSurfaceTrackerTest);
};

TEST_F(EmbeddedSurfaceTrackerTest, SetCurrentSurfaceId) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);

  // Initially HasValidSurfaceId() should be false.
  EXPECT_FALSE(tracker().HasValidSurfaceId());

  // After setting current SurfaceId then HasValidSurfaceId() should be true.
  tracker().SetCurrentSurfaceId(parent_id);
  EXPECT_TRUE(tracker().HasValidSurfaceId());
  EXPECT_EQ(parent_id, tracker().current_surface_id());
}

TEST_F(EmbeddedSurfaceTrackerTest, EmbedSurface) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId embedded_id = MakeSurfaceId(kEmbeddedFrameSink, 1);
  const SurfaceReference reference(parent_id, embedded_id);

  // Set parent surface id then embed another surface. The tracker should have a
  // reference to add from parent surface to embedded surface.
  tracker().SetCurrentSurfaceId(parent_id);
  EXPECT_TRUE(tracker().EmbedSurface(embedded_id));
  EXPECT_THAT(peek_to_add(), ElementsAre(reference));

  // Getting the references to add should return the correct reference and empty
  // the reference the add.
  EXPECT_THAT(tracker().GetReferencesToAdd(), ElementsAre(reference));
  EXPECT_THAT(peek_to_add(), IsEmpty());
}

TEST_F(EmbeddedSurfaceTrackerTest, EmbedNewSurfaceForFrameSink) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId embedded_id_first = MakeSurfaceId(kEmbeddedFrameSink, 1);
  const SurfaceId embedded_id_second = MakeSurfaceId(kEmbeddedFrameSink, 2);
  const SurfaceReference reference_first(parent_id, embedded_id_first);
  const SurfaceReference reference_second(parent_id, embedded_id_second);

  // Embed a surface and add references.
  tracker().SetCurrentSurfaceId(parent_id);
  EXPECT_TRUE(tracker().EmbedSurface(embedded_id_first));
  EXPECT_THAT(tracker().GetReferencesToAdd(), ElementsAre(reference_first));

  // Embed a newer surface with the same FrameSinkId as the first embedded
  // surface. The first reference should be in the list to remove and the new
  // reference in the last to add.
  EXPECT_FALSE(tracker().EmbedSurface(embedded_id_second));
  EXPECT_THAT(peek_to_remove(), ElementsAre(reference_first));
  EXPECT_THAT(peek_to_add(), ElementsAre(reference_second));
}

TEST_F(EmbeddedSurfaceTrackerTest, UnembedSurface) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId embedded_id = MakeSurfaceId(kEmbeddedFrameSink, 1);
  const SurfaceReference reference(parent_id, embedded_id);

  tracker().SetCurrentSurfaceId(parent_id);
  tracker().EmbedSurface(embedded_id);
  EXPECT_THAT(tracker().GetReferencesToAdd(), ElementsAre(reference));
  EXPECT_THAT(peek_to_remove(), IsEmpty());

  // Unembed the surface. The reference should be in the list to remove.
  tracker().UnembedSurface(embedded_id.frame_sink_id());
  EXPECT_THAT(peek_to_remove(), ElementsAre(reference));

  // Getting the references to remove should return the correct reference and
  // empty the reference the remove.
  EXPECT_THAT(tracker().GetReferencesToRemove(), ElementsAre(reference));
  EXPECT_THAT(peek_to_remove(), IsEmpty());
}

TEST_F(EmbeddedSurfaceTrackerTest, EmbedSurfaceBeforeSetCurrentSurfaceId) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId embedded_id = MakeSurfaceId(kEmbeddedFrameSink, 1);
  const SurfaceReference reference(parent_id, embedded_id);

  // Embed a surface before the parent id is set. There should be no references
  // to add because we don't know the parent id yet.
  tracker().EmbedSurface(embedded_id);
  EXPECT_THAT(peek_to_add(), IsEmpty());

  // Set the parent id, this should add a reference to add to the embedded
  // surface.
  tracker().SetCurrentSurfaceId(parent_id);
  EXPECT_THAT(peek_to_add(), ElementsAre(reference));
}

TEST_F(EmbeddedSurfaceTrackerTest, UpdateCurrentSurfaceId) {
  const SurfaceId parent_id_first = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id_second = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId embedded_id = MakeSurfaceId(kEmbeddedFrameSink, 1);
  const SurfaceReference reference(parent_id_second, embedded_id);

  // Set parent id and embed a surface.
  tracker().SetCurrentSurfaceId(parent_id_first);
  tracker().EmbedSurface(embedded_id);
  EXPECT_THAT(tracker().GetReferencesToAdd(), SizeIs(1));

  // Update the current parent id. There should be a reference to add from the
  // new parent id to the embedded surface.
  tracker().SetCurrentSurfaceId(parent_id_second);
  EXPECT_THAT(peek_to_add(), ElementsAre(reference));
}

TEST_F(EmbeddedSurfaceTrackerTest, UpdateCurrentSurfaceIdBeforeAdding) {
  const SurfaceId parent_id_first = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id_second = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId embedded_id = MakeSurfaceId(kEmbeddedFrameSink, 1);
  const SurfaceReference reference_first(parent_id_first, embedded_id);
  const SurfaceReference reference_second(parent_id_second, embedded_id);

  // Set parent id and embed a surface. There should be a reference to add from
  // the parent to embedded surface.
  tracker().SetCurrentSurfaceId(parent_id_first);
  tracker().EmbedSurface(embedded_id);
  EXPECT_THAT(peek_to_add(), ElementsAre(reference_first));

  // Update the parent id before sending IPC to add references. There should be
  // a reference to add from the new parent id to the embedded surface and the
  // previous reference to add was deleted.
  tracker().SetCurrentSurfaceId(parent_id_second);
  EXPECT_THAT(peek_to_add(), ElementsAre(reference_second));
  EXPECT_THAT(tracker().GetReferencesToAdd(), ElementsAre(reference_second));
}

}  // namespace test
}  // namespace cc
