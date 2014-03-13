// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_utils.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace content {

// static
bool ServiceWorkerUtils::IsFeatureEnabled() {
  static bool enabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableServiceWorker);
  return enabled;
}

}  // namespace content
