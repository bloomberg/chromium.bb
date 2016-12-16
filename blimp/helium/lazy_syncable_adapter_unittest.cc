// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/lazy_syncable_adapter.h"

#include <stdint.h>

#include "base/bind.h"
#include "blimp/helium/helium_test.h"
#include "blimp/helium/mock_objects.h"
#include "blimp/helium/serializable_struct.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

namespace blimp {
namespace helium {
namespace {

constexpr Revision kRevision = 42;

class LazySyncableAdapterTest : public HeliumTest {
 public:
  LazySyncableAdapterTest() : adapter_(&syncable_) {}
  ~LazySyncableAdapterTest() override = default;

  MOCK_METHOD0(CallbackMock, void());

 protected:
  MockSyncable syncable_;
  LazySyncableAdapter<TestSyncableChangeset> adapter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LazySyncableAdapterTest);
};

TEST_F(LazySyncableAdapterTest, ForwardsSetLocalCallback) {
  base::Closure callback_arg;

  EXPECT_CALL(syncable_, SetLocalUpdateCallback(_))
      .WillOnce(SaveArg<0>(&callback_arg));
  adapter_.SetLocalUpdateCallback(base::Bind(
      &LazySyncableAdapterTest::CallbackMock, base::Unretained(this)));

  EXPECT_CALL(*this, CallbackMock());
  callback_arg.Run();
}

TEST_F(LazySyncableAdapterTest, ForwardsReleaseBefore) {
  EXPECT_CALL(syncable_, ReleaseBefore(kRevision));
  adapter_.ReleaseBefore(kRevision);
}

TEST_F(LazySyncableAdapterTest, ForwardsGetRevision) {
  EXPECT_CALL(syncable_, GetRevision()).WillOnce(Return(kRevision));
  EXPECT_EQ(kRevision, adapter_.GetRevision());
}

TEST_F(LazySyncableAdapterTest, ForwardsPrepareToCreateChangeset) {
  base::Closure callback_arg;

  EXPECT_CALL(syncable_, PrepareToCreateChangeset(kRevision, _))
      .WillOnce(SaveArg<1>(&callback_arg));
  adapter_.PrepareToCreateChangeset(
      kRevision, base::Bind(&LazySyncableAdapterTest::CallbackMock,
                            base::Unretained(this)));

  EXPECT_CALL(*this, CallbackMock());
  callback_arg.Run();
}

TEST_F(LazySyncableAdapterTest, SerializesParsesForwardsChangesets) {
  std::unique_ptr<TestSyncableChangeset> changeset =
      base::MakeUnique<TestSyncableChangeset>();
  changeset->value.Set(33);

  EXPECT_CALL(syncable_, CreateChangesetMock(kRevision))
      .WillOnce(Return(changeset.release()));
  std::unique_ptr<std::string> string_changeset =
      adapter_.CreateChangeset(kRevision);

  TestSyncableChangeset validate_changeset;
  EXPECT_CALL(syncable_, ValidateChangeset(_))
      .WillOnce(DoAll(SaveArg<0>(&validate_changeset), Return(true)));
  EXPECT_EQ(true, adapter_.ValidateChangeset(*string_changeset));
  EXPECT_EQ(33, validate_changeset.value());

  TestSyncableChangeset apply_changeset;
  EXPECT_CALL(syncable_, ApplyChangeset(_))
      .WillOnce(SaveArg<0>(&apply_changeset));
  adapter_.ApplyChangeset(*string_changeset);
  EXPECT_EQ(33, apply_changeset.value());
}

}  // namespace
}  // namespace helium
}  // namespace blimp
