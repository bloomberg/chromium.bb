// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/reading_list_switches.h"

#include "base/command_line.h"

namespace reading_list {
namespace switches {
// Enables the reading list.
const char kEnableReadingList[] = "enable-reading-list";

// Disables the reading list.
const char kDisableReadingList[] = "disable-reading-list";

bool IsReadingListEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kEnableReadingList)) {
    return true;
  }
  return false;
}
}  // namespace switches
}  // namespace reading_list
