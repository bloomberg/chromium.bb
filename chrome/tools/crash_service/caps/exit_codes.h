// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_CRASH_SERVICE_CAPS_EXIT_CODES_H_
#define CHROME_TOOLS_CRASH_SERVICE_CAPS_EXIT_CODES_H_

namespace caps {

// Exit codes for CAPS. Don't reorder these values.
enum ExitCodes {
  EC_NORMAL_EXIT = 0,
  EC_EXISTING_INSTANCE,
  EC_INIT_ERROR,
  EC_LAST_CODE
};

}  // namespace caps

#endif  // CHROME_TOOLS_CRASH_SERVICE_CAPS_EXIT_CODES_H_

