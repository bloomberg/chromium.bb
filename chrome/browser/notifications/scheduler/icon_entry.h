// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_ICON_ENTRY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_ICON_ENTRY_H_

#include <string>
#include <utility>

#include "base/macros.h"

namespace notifications {

// The database entry that contains a notification icon, deserialized from the
// icon protobuffer.
// The icon can be a large chunk of memory so should be used in caution. The
// format of the data is the same as the format in the protobuffer, and may need
// to be converted to bitmap when used by the UI.
class IconEntry {
 public:
  using IconData = std::string;
  IconEntry(const std::string& uuid, IconData data);

  const std::string& uuid() const { return uuid_; }
  const IconData& data() const { return data_; }
  IconData release_data() { return std::move(data_); }

 private:
  const std::string uuid_;
  IconData data_;

  DISALLOW_COPY_AND_ASSIGN(IconEntry);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_ICON_ENTRY_H_
