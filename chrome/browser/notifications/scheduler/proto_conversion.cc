// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/proto_conversion.h"

#include <memory>
#include <utility>

#include "base/logging.h"

namespace notifications {

void IconEntryToProto(const IconEntry& entry,
                      notifications::proto::Icon* proto) {
  proto->set_uuid(entry.uuid);

  // Copy large chunk of data to proto.
  proto->set_icon(entry.data);
}

void IconProtoToEntry(const proto::Icon& proto,
                      notifications::IconEntry* entry) {
  DCHECK(proto.has_uuid());
  DCHECK(proto.has_icon());
  entry->data = proto.icon();
  entry->uuid = proto.uuid();
}

}  // namespace notifications
