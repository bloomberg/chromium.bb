// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/logging.h"

#include "base/location.h"

namespace browser_sync {

bool VlogIsOnForLocation(const tracked_objects::Location& from_here,
                         int verbose_level) {
  return (verbose_level <=
          logging::GetVlogLevelHelper(
              from_here.file_name(), ::strlen(from_here.file_name())));
}

}  // namespace browser_sync
