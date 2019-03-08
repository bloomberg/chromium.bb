// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/proto_conversion.h"

#include <memory>
#include <utility>

#include "base/logging.h"

namespace notifications {

proto::Icon IconEntryToProto(const IconEntry& entry) {
  proto::Icon proto;
  proto.set_uuid(entry.uuid());

  // Move large chunk of data from entry to the proto.
  proto.set_icon(entry.data());
  return proto;
}

std::unique_ptr<IconEntry> IconProtoToEntry(proto::Icon& proto) {
  DCHECK(proto.has_uuid());
  DCHECK(proto.has_icon());

  // Move large chunk of data from proto to entry.
  std::string* proto_icon = proto.release_icon();
  auto icon_entry =
      std::make_unique<IconEntry>(proto.uuid(), std::move(*proto_icon));
  return icon_entry;
}

}  // namespace notifications
