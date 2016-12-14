// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_session_observer.h"

namespace arc {

std::ostream& operator<<(std::ostream& os,
                         ArcSessionObserver::StopReason reason) {
  switch (reason) {
#define CASE_IMPL(val)                      \
  case ArcSessionObserver::StopReason::val: \
    return os << #val

    CASE_IMPL(SHUTDOWN);
    CASE_IMPL(GENERIC_BOOT_FAILURE);
    CASE_IMPL(LOW_DISK_SPACE);
    CASE_IMPL(CRASH);
#undef CASE_IMPL
  }

  // In case of unexpected value, output the int value.
  return os << "StopReason(" << static_cast<int>(reason) << ")";
}

}  // namespace arc
