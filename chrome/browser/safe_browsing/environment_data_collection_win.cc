// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/environment_data_collection_win.h"

#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

void CollectPlatformProcessData(
    ClientIncidentReport_EnvironmentData_Process* process) {
  // TODO(pmonette): collect dlls and lsps.
}

}  // namespace safe_browsing
