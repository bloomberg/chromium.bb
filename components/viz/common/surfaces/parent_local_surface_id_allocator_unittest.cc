// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

// ParentLocalSurfaceIdAllocator has 2 accessors which do not alter state:
// - GetCurrentLocalSurfaceId()
// - is_allocation_suppressed()
//
// For every operation which changes state we can test:
// - the operation completed as expected,
// - the accessors did not change, and/or
// - the accessors changed in the way we expected.

namespace viz {
namespace {

::testing::AssertionResult ParentSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id);
::testing::AssertionResult ChildSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id);
::testing::AssertionResult EmbedTokenIsValid(
    const LocalSurfaceId& local_surface_id);

}  // namespace

class ParentLocalSurfaceIdAllocatorTest : public testing::Test {
 public:
  ParentLocalSurfaceIdAllocatorTest() = default;

  ~ParentLocalSurfaceIdAllocatorTest() override {}

  ParentLocalSurfaceIdAllocator& allocator() { return *allocator_.get(); }

  base::TimeTicks Now() { return now_src_->NowTicks(); }

  void AdvanceTime(base::TimeDelta delta) { now_src_->Advance(delta); }

  LocalSurfaceId GenerateChildLocalSurfaceId() {
    const LocalSurfaceId& current_local_surface_id =
        allocator_->GetCurrentLocalSurfaceId();

    return LocalSurfaceId(current_local_surface_id.parent_sequence_number(),
                          current_local_surface_id.child_sequence_number() + 1,
                          current_local_surface_id.embed_token());
  }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    now_src_ = std::make_unique<base::SimpleTestTickClock>();
    allocator_ =
        std::make_unique<ParentLocalSurfaceIdAllocator>(now_src_.get());
  }

  void TearDown() override {
    allocator_.reset();
    now_src_.reset();
  }

 private:
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  std::unique_ptr<ParentLocalSurfaceIdAllocator> allocator_;

  DISALLOW_COPY_AND_ASSIGN(ParentLocalSurfaceIdAllocatorTest);
};

// The default constructor should generate a embed_token and initialize the
// last known LocalSurfaceId. Allocation should not be suppressed.
TEST_F(ParentLocalSurfaceIdAllocatorTest,
       DefaultConstructorShouldInitializeLocalSurfaceIdAndNotBeSuppressed) {
  const LocalSurfaceId& default_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();
  EXPECT_TRUE(default_local_surface_id.is_valid());
  EXPECT_TRUE(ParentSequenceNumberIsSet(default_local_surface_id));
  EXPECT_TRUE(ChildSequenceNumberIsSet(default_local_surface_id));
  EXPECT_TRUE(EmbedTokenIsValid(default_local_surface_id));
  EXPECT_FALSE(allocator().is_allocation_suppressed());
}

// UpdateFromChild() on a parent allocator should accept the child's sequence
// number. But it should continue to use its own parent sequence number and
// embed_token.
TEST_F(ParentLocalSurfaceIdAllocatorTest,
       UpdateFromChildOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  allocator().GenerateId();
  LocalSurfaceId preupdate_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();
  LocalSurfaceId child_allocated_local_surface_id =
      GenerateChildLocalSurfaceId();
  EXPECT_EQ(preupdate_local_surface_id.parent_sequence_number(),
            child_allocated_local_surface_id.parent_sequence_number());
  EXPECT_NE(preupdate_local_surface_id.child_sequence_number(),
            child_allocated_local_surface_id.child_sequence_number());
  EXPECT_EQ(preupdate_local_surface_id.embed_token(),
            child_allocated_local_surface_id.embed_token());

  bool changed = allocator().UpdateFromChild(child_allocated_local_surface_id,
                                             base::TimeTicks());
  EXPECT_TRUE(changed);

  const LocalSurfaceId& postupdate_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();
  EXPECT_EQ(postupdate_local_surface_id.parent_sequence_number(),
            child_allocated_local_surface_id.parent_sequence_number());
  EXPECT_EQ(postupdate_local_surface_id.child_sequence_number(),
            child_allocated_local_surface_id.child_sequence_number());
  EXPECT_EQ(postupdate_local_surface_id.embed_token(),
            child_allocated_local_surface_id.embed_token());
  EXPECT_FALSE(allocator().is_allocation_suppressed());
}

// GenerateId() on a parent allocator should monotonically increment the parent
// sequence number and use the previous embed_token.
TEST_F(ParentLocalSurfaceIdAllocatorTest,
       GenerateIdOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  LocalSurfaceId pregenerateid_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();

  allocator().GenerateId();
  const LocalSurfaceId& returned_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();

  const LocalSurfaceId& postgenerateid_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();
  EXPECT_EQ(pregenerateid_local_surface_id.parent_sequence_number() + 1,
            postgenerateid_local_surface_id.parent_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.child_sequence_number(),
            postgenerateid_local_surface_id.child_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.embed_token(),
            postgenerateid_local_surface_id.embed_token());
  EXPECT_EQ(returned_local_surface_id, allocator().GetCurrentLocalSurfaceId());
  EXPECT_FALSE(allocator().is_allocation_suppressed());
}

// This test verifies that calling reset with a LocalSurfaceId updates the
// GetCurrentLocalSurfaceId and affects GenerateId.
TEST_F(ParentLocalSurfaceIdAllocatorTest, ResetUpdatesComponents) {
  LocalSurfaceId default_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();
  EXPECT_TRUE(default_local_surface_id.is_valid());
  EXPECT_TRUE(ParentSequenceNumberIsSet(default_local_surface_id));
  EXPECT_TRUE(ChildSequenceNumberIsSet(default_local_surface_id));
  EXPECT_TRUE(EmbedTokenIsValid(default_local_surface_id));

  LocalSurfaceId new_local_surface_id(
      2u, 2u, base::UnguessableToken::Deserialize(0, 1u));

  allocator().Reset(new_local_surface_id);
  EXPECT_EQ(new_local_surface_id, allocator().GetCurrentLocalSurfaceId());

  allocator().GenerateId();
  LocalSurfaceId generated_id = allocator().GetCurrentLocalSurfaceId();

  EXPECT_EQ(generated_id.embed_token(), new_local_surface_id.embed_token());
  EXPECT_EQ(generated_id.child_sequence_number(),
            new_local_surface_id.child_sequence_number());
  EXPECT_EQ(generated_id.parent_sequence_number(),
            new_local_surface_id.child_sequence_number() + 1);
}

// This test verifies that if the child-allocated LocalSurfaceId has the most
// recent parent sequence number at the time UpdateFromChild is called, then
// its allocation time is used as the latest allocation time in
// ParentLocalSurfaceIdAllocator. In the event that the child-allocated
// LocalSurfaceId does not correspond to the latest parent sequence number
// then UpdateFromChild represents a new allocation and thus the allocation time
// is updated.
TEST_F(ParentLocalSurfaceIdAllocatorTest,
       CorrectTimeStampUsedInUpdateFromChild) {
  LocalSurfaceId child_allocated_id = GenerateChildLocalSurfaceId();
  base::TimeTicks child_allocation_time = Now();

  // Advance time by one millisecond.
  AdvanceTime(base::TimeDelta::FromMilliseconds(1u));

  {
    bool changed =
        allocator().UpdateFromChild(child_allocated_id, child_allocation_time);
    EXPECT_TRUE(changed);
    EXPECT_EQ(child_allocated_id, allocator().GetCurrentLocalSurfaceId());
    EXPECT_EQ(child_allocation_time, allocator().allocation_time());
  }

  LocalSurfaceId child_allocated_id2 = GenerateChildLocalSurfaceId();
  allocator().GenerateId();
  {
    bool changed =
        allocator().UpdateFromChild(child_allocated_id2, child_allocation_time);
    EXPECT_TRUE(changed);
    EXPECT_NE(child_allocated_id2, allocator().GetCurrentLocalSurfaceId());
    EXPECT_EQ(child_allocated_id2.child_sequence_number(),
              allocator().GetCurrentLocalSurfaceId().child_sequence_number());
    EXPECT_EQ(Now(), allocator().allocation_time());
  }
}

namespace {

::testing::AssertionResult ParentSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id) {
  if (local_surface_id.parent_sequence_number() != kInvalidParentSequenceNumber)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure()
         << "parent_sequence_number() is not set.";
}

::testing::AssertionResult ChildSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id) {
  if (local_surface_id.child_sequence_number() != kInvalidChildSequenceNumber)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "child_sequence_number() is not set.";
}

::testing::AssertionResult EmbedTokenIsValid(
    const LocalSurfaceId& local_surface_id) {
  if (!local_surface_id.embed_token().is_empty())
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "embed_token() is invalid.";
}

}  // namespace
}  // namespace viz
