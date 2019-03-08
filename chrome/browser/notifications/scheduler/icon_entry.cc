// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/notifications/scheduler/icon_entry.h"

namespace notifications {

IconEntry::IconEntry(const std::string& uuid, IconData data)
    : uuid_(uuid), data_(std::move(data)) {}

}  // namespace notifications
