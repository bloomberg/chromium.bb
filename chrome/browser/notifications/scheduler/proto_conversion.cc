// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/proto_conversion.h"

#include <memory>
#include <utility>

#include "base/logging.h"

namespace notifications {

void IconEntryToProto(IconEntry* entry, notifications::proto::Icon* proto) {
  proto->mutable_uuid()->swap(entry->uuid);
  proto->mutable_icon()->swap(entry->data);
}

void IconProtoToEntry(proto::Icon* proto, notifications::IconEntry* entry) {
  DCHECK(proto->has_uuid());
  DCHECK(proto->has_icon());
  entry->data.swap(*proto->mutable_icon());
  entry->uuid.swap(*proto->mutable_uuid());
}

}  // namespace notifications
