// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/proto_conversion.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

using IconProto = notifications::proto::Icon;

namespace notifications {
namespace {

const char kUuid[] = "123";
const char kData[] = "bitmapdata";

TEST(ProtoConversionTest, IconProtoToEntry) {
  IconProto proto;
  proto.set_uuid(kUuid);
  proto.set_icon(kData);
  auto entry = IconProtoToEntry(proto);

  // Verify entry data.
  DCHECK(entry);
  EXPECT_EQ(entry->uuid(), kUuid);
  EXPECT_EQ(entry->data(), kData);

  // The data in proto should be moved to entry.
  EXPECT_TRUE(proto.icon().empty());
}

TEST(ProtoConversionTest, IconEntryToProto) {
  IconEntry entry(kUuid, kData);
  auto proto = IconEntryToProto(entry);

  // Verify proto data.
  EXPECT_EQ(proto.icon(), kData);
  EXPECT_EQ(proto.uuid(), kUuid);

  // Icon data in entry is copied to the protobuffer.
  EXPECT_EQ(entry.data(), kData);
}

}  // namespace
}  // namespace notifications
