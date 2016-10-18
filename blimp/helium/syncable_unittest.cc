// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/syncable.h"

#include <utility>

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "blimp/common/mandatory_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace blimp {
namespace helium {
namespace {

// This is a sample implementation that demostrates the implementation
// of the Syncable and TwoPhaseSyncable
// For simplicity of this example, the ChangeSet will be an integer.
class FakeIntSyncable : public Syncable {
 public:
  explicit FakeIntSyncable(VersionVectorGenerator* clock_gen)
      : clock_gen_(clock_gen), value_(0) {
    last_modified_ = clock_gen_->current();
  }

  bool ModifiedSince(const VersionVector& from) const override {
    return from.local_revision() < last_modified_.local_revision();
  }

  void CreateChangesetToCurrent(
      const VersionVector& from,
      google::protobuf::io::CodedOutputStream* output_stream) override {
    output_stream->WriteVarint32(value_);
  }

  void ApplyChangeset(
      const VersionVector& from,
      const VersionVector& to,
      google::protobuf::io::CodedInputStream* input_stream) override {
    input_stream->ReadVarint32(&value_);

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
  uint32_t value_;

  DISALLOW_COPY_AND_ASSIGN(FakeIntSyncable);
};

class ParentObjectSyncable : public TwoPhaseSyncable {
 public:
  explicit ParentObjectSyncable(VersionVectorGenerator* clock_gen)
      : TwoPhaseSyncable(), child1_(clock_gen), child2_(clock_gen) {}

  void CreateChangesetToCurrent(
      const VersionVector& from,
      google::protobuf::io::CodedOutputStream* output_stream) override {
    // Low-level serialization.
    // Field format: <field count> (<field id> <varint>)*.
    int count = child1_.ModifiedSince(from) + child2_.ModifiedSince(from);
    output_stream->WriteVarint32(count);
    if (child1_.ModifiedSince(from)) {
      output_stream->WriteTag(1);
      child1_.CreateChangesetToCurrent(from, output_stream);
    }

    if (child2_.ModifiedSince(from)) {
      output_stream->WriteTag(2);
      child2_.CreateChangesetToCurrent(from, output_stream);
    }
  }

  void ApplyChangeset(
      const VersionVector& from,
      const VersionVector& to,
      google::protobuf::io::CodedInputStream* input_stream) override {
    uint32_t count;
    CHECK(input_stream->ReadVarint32(&count));
    CHECK_GT(count, 0u);

    for (uint32_t i = 0; i < count; ++i) {
      switch (input_stream->ReadTag()) {
        case 1:
          child1_.ApplyChangeset(from, to, input_stream);
          break;
        case 2:
          child2_.ApplyChangeset(from, to, input_stream);
          break;
      }
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

  FakeIntSyncable* mutable_child1() { return &child1_; }
  FakeIntSyncable* mutable_child2() { return &child2_; }

 private:
  FakeIntSyncable child1_;
  FakeIntSyncable child2_;

  DISALLOW_COPY_AND_ASSIGN(ParentObjectSyncable);
};

class SyncableTest : public testing::Test {
 public:
  SyncableTest()
      : last_sync_master_(master_clock_.current()),
        last_sync_replica_(replica_clock_.current()),
        master_(&master_clock_),
        replica_(&replica_clock_) {}

  ~SyncableTest() override {}

 protected:
  // Propagates pending changes from |master_| to |replica_|.
  void Synchronize() {
    EXPECT_TRUE(master_.ModifiedSince(last_sync_master_));
    EXPECT_FALSE(replica_.ModifiedSince(last_sync_replica_));

    // Create the changeset stream from |master_|.
    std::string changeset;
    google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
    google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);

    master_.CreateChangesetToCurrent(last_sync_master_, &output_stream);
    CHECK(!changeset.empty());
    output_stream.Trim();

    // Apply the changeset stream to |replica_|.
    google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                            changeset.size());
    google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);

    replica_.ApplyChangeset(last_sync_replica_, replica_clock_.current(),
                            &input_stream);
    last_sync_master_ = master_clock_.current();
    master_.ReleaseCheckpointsBefore(last_sync_master_);

    // Ensure that the changeset stream was consumed in its entirety.
    EXPECT_FALSE(master_.ModifiedSince(last_sync_master_));
    EXPECT_FALSE(replica_.ModifiedSince(last_sync_replica_));
    EXPECT_FALSE(input_stream.Skip(1));
  }

  VersionVectorGenerator master_clock_;
  VersionVectorGenerator replica_clock_;
  VersionVector last_sync_master_;
  VersionVector last_sync_replica_;
  ParentObjectSyncable master_;
  ParentObjectSyncable replica_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncableTest);
};

TEST_F(SyncableTest, SequentialMutations) {
  master_.mutable_child1()->SetValue(123);
  Synchronize();
  EXPECT_EQ(123, replica_.mutable_child1()->value());

  master_.mutable_child1()->SetValue(456);
  Synchronize();
  EXPECT_EQ(456, replica_.mutable_child1()->value());
}

TEST_F(SyncableTest, MutateMultiple) {
  master_.mutable_child1()->SetValue(123);
  master_.mutable_child2()->SetValue(456);
  Synchronize();
  EXPECT_EQ(123, replica_.mutable_child1()->value());
  EXPECT_EQ(456, replica_.mutable_child2()->value());
}

TEST_F(SyncableTest, MutateMultipleDiscrete) {
  master_.mutable_child1()->SetValue(123);
  Synchronize();
  EXPECT_EQ(123, replica_.mutable_child1()->value());

  master_.mutable_child2()->SetValue(456);
  Synchronize();
  EXPECT_EQ(123, replica_.mutable_child1()->value());
  EXPECT_EQ(456, replica_.mutable_child2()->value());
}

}  // namespace
}  // namespace helium
}  // namespace blimp
