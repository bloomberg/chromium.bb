// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DELAYED_ANALYSIS_CALLBACK_H_
#define CHROME_BROWSER_SAFE_BROWSING_DELAYED_ANALYSIS_CALLBACK_H_

#include "base/callback_forward.h"
#include "chrome/browser/safe_browsing/add_incident_callback.h"

namespace safe_browsing {

// A callback used by external components to register a process-wide analysis
// step. The callback will be run after some delay following process launch in
// the blocking pool. The argument is a callback by which the consumer can add
// incidents to the incident reporting service.
typedef base::Callback<void(const AddIncidentCallback&)>
    DelayedAnalysisCallback;

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DELAYED_ANALYSIS_CALLBACK_H_
