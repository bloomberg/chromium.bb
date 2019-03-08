// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PROTO_CONVERSION_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PROTO_CONVERSION_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/notifications/proto/icon.pb.h"
#include "chrome/browser/notifications/scheduler/icon_entry.h"

namespace notifications {

// Converts an icon entry to icon proto.
proto::Icon IconEntryToProto(const IconEntry& entry);

// Converts an icon proto to icon entry.
std::unique_ptr<IconEntry> IconProtoToEntry(proto::Icon& proto);

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PROTO_CONVERSION_H_
