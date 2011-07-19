// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/nudge_source.h"

#include "base/logging.h"

namespace browser_sync {

#define ENUM_CASE(x) case x: return #x; break

const char* GetNudgeSourceString(NudgeSource nudge_source) {
  switch (nudge_source) {
    ENUM_CASE(NUDGE_SOURCE_UNKNOWN);
    ENUM_CASE(NUDGE_SOURCE_NOTIFICATION);
    ENUM_CASE(NUDGE_SOURCE_LOCAL);
    ENUM_CASE(NUDGE_SOURCE_CONTINUATION);
  };
  NOTREACHED();
  return "";
}

#undef ENUM_CASE

}  // namespace browser_sync
