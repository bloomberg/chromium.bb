// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include "services/service_manager/runner/common/client_util.h"

bool IsFullScreenMode(int64_t display_id) {
  if (service_manager::ServiceManagerIsRemote()) {
    // TODO: http://crbug.com/640390.
    NOTIMPLEMENTED();
    return false;
  }

  NOTREACHED() << "For Ozone builds, only --mash launch is supported for now.";
  return false;
}
