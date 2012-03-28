// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "chrome/common/chrome_constants.h"

namespace browser_sync {

void ChromeReportUnrecoverableError() {
  // This currently works on windows platform but is disabled for now.
  // TODO(lipalani): Add this for other platforms as well.
  // TODO(lipalani) : Re-enable error reporting on trunk.
}

}  // namespace browser_sync
