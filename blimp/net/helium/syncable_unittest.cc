// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/helium/syncable.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "blimp/common/mandatory_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

// This is a sample implementation that demostrates the implementation
// of the Syncable and TwoPhaseSyncable

// For simplicity of this example, the ChangeSet will be an integer.
class FakeIntSyncable : public Syncable<int> {
 public:
  explicit FakeIntSyncable(VersionVectorGenerator* clock_gen)
      : Syncable<int>(), clock_gen_(clock_gen), value_(0) {
    last_modified_ = clock_gen_->current();
  }

  bool ModifiedSince(const VersionVector& from) const override {
    return from.local_revision() < last_modified_.local_revision();
  }

  std::unique_ptr<int> CreateChangesetToCurrent(
      const VersionVector& from) override {
    return base::MakeUnique<int>(value_);
  }

  void ApplyChangeset(const VersionVector& from,
                      const VersionVector& to,
                      std::unique_ptr<int> changeset) override {
    // Restore the value
    value_ = *changeset;

    // Update our clock to the latest clock
    last_modified_ = to;
  }

  void ReleaseCheckpointsBefore(const VersionVector& checkpoint) override {
    last_modified_ = checkpoint;
  }

  void SetValue(int value) {
    value_ = value;

    // Increment the parent clock and update our last_modified_ value
    clock_gen_->Increment();
    last_modified_ = clock_gen_->current();
  }

  int value() { return value_; }

 private:
  // The last time this object was changed
  VersionVectorGenerator* clock_gen_;
  VersionVector last_modified_;
  int32_t value_;

  DISALLOW_COPY_AND_ASSIGN(FakeIntSyncable);
};

class ParentObjectSyncable : public TwoPhaseSyncable {
 public:
  explicit ParentObjectSyncable(VersionVectorGenerator* clock_gen)
      : TwoPhaseSyncable(), child1_(clock_gen), child2_(clock_gen) {}

  std::unique_ptr<proto::ChangesetMessage> CreateChangesetToCurrent(
      const VersionVector& from) override {
    std::unique_ptr<proto::ChangesetMessage> changeset =
        base::MakeUnique<proto::ChangesetMessage>();

    proto::TestChangesetMessage* bm = changeset->mutable_test();

    if (child1_.ModifiedSince(from)) {
      std::unique_ptr<int> value1 = child1_.CreateChangesetToCurrent(from);
      bm->set_value1(*value1);
    }

    if (child2_.ModifiedSince(from)) {
      std::unique_ptr<int> value2 = child2_.CreateChangesetToCurrent(from);
      bm->set_value2(*value2);
    }

    return changeset;
  }

  void ApplyChangeset(
      const VersionVector& from,
      const VersionVector& to,
      std::unique_ptr<proto::ChangesetMessage> changeset) override {
    proto::TestChangesetMessage bm = changeset->test();

    int child1_value = bm.value1();
    if (child1_value != 0) {
      child1_.ApplyChangeset(from, to, base::MakeUnique<int>(child1_value));
    }

    int child2_value = bm.value2();
    if (child2_value != 0) {
      child2_.ApplyChangeset(from, to, base::MakeUnique<int>(child2_value));
    }
  }

  void ReleaseCheckpointsBefore(const VersionVector& checkpoint) override {
    child1_.ReleaseCheckpointsBefore(checkpoint);
    child2_.ReleaseCheckpointsBefore(checkpoint);
  }

  bool ModifiedSince(const VersionVector& from) const override {
    return child1_.ModifiedSince(from) || child2_.ModifiedSince(from);
  }

  void PreCreateChangesetToCurrent(const VersionVector& from,
                                   MandatoryClosure&& done) override {
    done.Run();
  }

  void PostApplyChangeset(const VersionVector& from,
                          const VersionVector& to,
                          MandatoryClosure&& done) override {
    done.Run();
  }

  FakeIntSyncable* get_mutable_child1() { return &child1_; }
  FakeIntSyncable* get_mutable_child2() { return &child2_; }

 private:
  FakeIntSyncable child1_;
  FakeIntSyncable child2_;

  DISALLOW_COPY_AND_ASSIGN(ParentObjectSyncable);
};

class SyncableTest : public testing::Test {
 public:
  SyncableTest()
      : clock_gen1_(base::MakeUnique<VersionVectorGenerator>()),
        clock_gen2_(base::MakeUnique<VersionVectorGenerator>()),
        last_sync_local_(clock_gen1_->current()),
        last_sync_remote_(clock_gen2_->current()),
        parent_local_(clock_gen1_.get()),
        parent_remote_(clock_gen2_.get()) {}

  ~SyncableTest() override {}

 protected:
  void ChangeChild(int value1_set,
                   int value1_get,
                   int value2_get,
                   FakeIntSyncable* child_to_be_modified,
                   FakeIntSyncable* child_to_be_read_only) {
    // Lets modify a children object
    child_to_be_modified->SetValue(value1_set);

    // At this point |child1| and |parent_local_| should have its clock
    // incremented whereas |child2| should still be the same.
    EXPECT_TRUE(child_to_be_modified->ModifiedSince(last_sync_local_));
    EXPECT_FALSE(child_to_be_read_only->ModifiedSince(last_sync_local_));

    std::unique_ptr<proto::ChangesetMessage> changeset =
        parent_local_.CreateChangesetToCurrent(last_sync_local_);

    VersionVector local_clock = clock_gen1_->current();
    VersionVector remote_clock = local_clock.Invert();

    parent_remote_.ApplyChangeset(last_sync_remote_, remote_clock,
                                  std::move(changeset));
    last_sync_local_ = local_clock;
    parent_local_.ReleaseCheckpointsBefore(local_clock);
    EXPECT_FALSE(child_to_be_modified->ModifiedSince(last_sync_local_));
    EXPECT_FALSE(child_to_be_read_only->ModifiedSince(last_sync_local_));

    EXPECT_EQ(value1_get, child_to_be_modified->value());
    EXPECT_EQ(value2_get, child_to_be_read_only->value());
  }

  std::unique_ptr<VersionVectorGenerator> clock_gen1_;
  std::unique_ptr<VersionVectorGenerator> clock_gen2_;
  VersionVector last_sync_local_;
  VersionVector last_sync_remote_;
  ParentObjectSyncable parent_local_;
  ParentObjectSyncable parent_remote_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncableTest);
};

TEST_F(SyncableTest, CreateAndApplyChangesetTest) {
  ChangeChild(123, 123, 0, parent_local_.get_mutable_child1(),
              parent_local_.get_mutable_child2());
  ChangeChild(456, 456, 123, parent_local_.get_mutable_child2(),
              parent_local_.get_mutable_child1());
}

}  // namespace
}  // namespace blimp
