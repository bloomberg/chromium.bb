// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/mock_objects.h"

namespace blimp {
namespace helium {

TestSyncableChangeset::TestSyncableChangeset() : value(this) {}

TestSyncableChangeset::~TestSyncableChangeset() {}

TestSyncableChangeset& TestSyncableChangeset::operator=(
    const TestSyncableChangeset& other) {
  value.Set(other.value());
  return *this;
}

MockSyncable::MockSyncable() = default;

MockSyncable::~MockSyncable() = default;

std::unique_ptr<TestSyncableChangeset> MockSyncable::CreateChangeset(
    Revision from) const {
  return base::WrapUnique<TestSyncableChangeset>(CreateChangesetMock(from));
}

MockStreamPump::MockStreamPump() = default;

MockStreamPump::~MockStreamPump() = default;

}  // namespace helium
}  // namespace blimp
