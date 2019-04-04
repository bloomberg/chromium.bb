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
  IconEntry entry;

  IconProtoToEntry(&proto, &entry);

  // Verify entry data.
  EXPECT_EQ(entry.uuid, kUuid);
  EXPECT_EQ(entry.data, kData);
}

TEST(ProtoConversionTest, IconEntryToProto) {
  IconEntry entry;
  entry.data = kData;
  entry.uuid = kUuid;
  IconProto proto;

  IconEntryToProto(&entry, &proto);

  // Verify proto data.
  EXPECT_EQ(proto.icon(), kData);
  EXPECT_EQ(proto.uuid(), kUuid);
}

}  // namespace
}  // namespace notifications
