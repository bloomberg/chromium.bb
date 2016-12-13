// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_switches.h"

#include "build/build_config.h"
#include "base/command_line.h"
#include "components/reading_list/core/reading_list_enable_flags.h"

namespace reading_list {
namespace switches {
bool IsReadingListEnabled() {
  return BUILDFLAG(ENABLE_READING_LIST);
}
}  // namespace switches
}  // namespace reading_list
