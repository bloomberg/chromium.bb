// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/compound_syncable.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "blimp/helium/helium_test.h"
#include "blimp/helium/lww_register.h"
#include "blimp/helium/revision_generator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace blimp {
namespace helium {
namespace {

struct SampleCompoundSyncable : public CompoundSyncable {
  explicit SampleCompoundSyncable(Peer bias, Peer running_as)
      : child1(CreateAndRegister<LwwRegister<int>>(bias, running_as)),
        child2(CreateAndRegister<LwwRegister<int>>(bias, running_as)) {}

  RegisteredSyncable<LwwRegister<int>> child1;
  RegisteredSyncable<LwwRegister<int>> child2;

 private:
  DISALLOW_COPY_AND_ASSIGN(SampleCompoundSyncable);
};

class CompoundSyncableTest : public HeliumTest {
 public:
  CompoundSyncableTest()
      : last_sync_engine_(0),
        engine_(Peer::ENGINE, Peer::ENGINE),
        client_(Peer::ENGINE, Peer::CLIENT) {
    client_.SetLocalUpdateCallback(base::Bind(
        &CompoundSyncableTest::OnClientCallbackCalled, base::Unretained(this)));
    engine_.SetLocalUpdateCallback(base::Bind(
        &CompoundSyncableTest::OnEngineCallbackCalled, base::Unretained(this)));
  }

  ~CompoundSyncableTest() override {}

 public:
  MOCK_METHOD0(OnEngineCallbackCalled, void());
  MOCK_METHOD0(OnClientCallbackCalled, void());

 protected:
  // Propagates pending changes between |engine_| and |client_|.
  void Synchronize() {
    ASSERT_TRUE(
        client_.ValidateChangeset(*engine_.CreateChangeset(last_sync_client_)));
    ASSERT_TRUE(
        engine_.ValidateChangeset(*client_.CreateChangeset(last_sync_engine_)));

    client_.ApplyChangeset(*engine_.CreateChangeset(last_sync_client_));
    engine_.ApplyChangeset(*client_.CreateChangeset(last_sync_engine_));

    last_sync_engine_ = RevisionGenerator::GetInstance()->current();
    last_sync_client_ = RevisionGenerator::GetInstance()->current();
  }

  Revision last_sync_engine_ = 0;
  Revision last_sync_client_ = 0;
  SampleCompoundSyncable engine_;
  SampleCompoundSyncable client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompoundSyncableTest);
};

TEST_F(CompoundSyncableTest, SequentialMutations) {
  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(0);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(2);

  engine_.child1->Set(123);
  Synchronize();
  EXPECT_EQ(123, client_.child1->Get());

  engine_.child1->Set(456);
  Synchronize();
  EXPECT_EQ(456, client_.child1->Get());
}

TEST_F(CompoundSyncableTest, NoOp) {
  Synchronize();

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(0);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(1);

  engine_.child1->Set(123);
  Synchronize();
  EXPECT_EQ(123, client_.child1->Get());

  Synchronize();
}

TEST_F(CompoundSyncableTest, MutateMultiple) {
  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(0);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(2);

  engine_.child1->Set(123);
  engine_.child2->Set(456);
  Synchronize();
  EXPECT_EQ(123, client_.child1->Get());
  EXPECT_EQ(456, client_.child2->Get());
}

TEST_F(CompoundSyncableTest, MutateMultipleDiscrete) {
  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(2);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(0);

  client_.child1->Set(123);
  Synchronize();
  EXPECT_EQ(123, client_.child1->Get());
  client_.child2->Set(456);
  Synchronize();
  EXPECT_EQ(123, engine_.child1->Get());
  EXPECT_EQ(456, engine_.child2->Get());
}

}  // namespace
}  // namespace helium
}  // namespace blimp
