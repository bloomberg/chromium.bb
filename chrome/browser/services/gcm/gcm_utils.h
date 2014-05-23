// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_UTILS_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_UTILS_H_

#include "google_apis/gcm/gcm_client.h"

namespace gcm {

// Returns the chrome build info that is passed to GCM.
GCMClient::ChromeBuildInfo GetChromeBuildInfo();

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_UTILS_H_
