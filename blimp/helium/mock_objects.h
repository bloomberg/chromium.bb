// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_MOCK_OBJECTS_H_
#define BLIMP_HELIUM_MOCK_OBJECTS_H_

#include "base/memory/ptr_util.h"
#include "blimp/helium/serializable_struct.h"
#include "blimp/helium/syncable.h"
#include "blimp/helium/update_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blimp {
namespace helium {

struct TestSyncableChangeset : public SerializableStruct {
  TestSyncableChangeset();
  ~TestSyncableChangeset() override;

  TestSyncableChangeset& operator=(const TestSyncableChangeset& other);

  Field<int32_t> value;
};

class MockSyncable : public LazySyncable<TestSyncableChangeset> {
 public:
  MockSyncable();
  ~MockSyncable() override;

  MOCK_CONST_METHOD1(CreateChangesetMock, TestSyncableChangeset*(Revision));
  MOCK_METHOD1(ApplyChangeset, void(const TestSyncableChangeset&));
  MOCK_CONST_METHOD1(ValidateChangeset, bool(const TestSyncableChangeset&));
  MOCK_METHOD1(ReleaseBefore, void(Revision));
  MOCK_CONST_METHOD0(GetRevision, Revision());
  MOCK_METHOD2(PrepareToCreateChangeset, void(Revision, base::Closure));
  MOCK_METHOD1(SetLocalUpdateCallback, void(const base::Closure&));

  std::unique_ptr<TestSyncableChangeset> CreateChangeset(Revision from) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSyncable);
};

class MockStreamPump : public StreamPump {
 public:
  MockStreamPump();
  ~MockStreamPump() override;

  MOCK_METHOD0(OnMessageAvailable, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStreamPump);
};

}  // namespace helium
}  // namespace blimp
#endif  // BLIMP_HELIUM_MOCK_OBJECTS_H_
