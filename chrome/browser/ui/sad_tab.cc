// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sad_tab.h"

namespace chrome {

// static
bool SadTab::ShouldShow(base::TerminationStatus status) {
  return (status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
          status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ||
          status == base::TERMINATION_STATUS_PROCESS_CRASHED);
}

}  // namespace chrome
