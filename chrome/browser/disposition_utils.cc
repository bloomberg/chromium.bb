// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/disposition_utils.h"

#include "build/build_config.h"

namespace disposition_utils {

WindowOpenDisposition DispositionFromClick(bool middle_button,
                                           bool alt_key,
                                           bool ctrl_key,
                                           bool meta_key,
                                           bool shift_key) {
  // MacOS uses meta key (Command key) to spawn new tabs.
#if defined(OS_MACOSX)
  if (middle_button || meta_key)
#else
  if (middle_button || ctrl_key)
#endif
    return shift_key ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  if (shift_key)
    return NEW_WINDOW;
  if (alt_key)
    return SAVE_TO_DISK;
  return CURRENT_TAB;
}

}
