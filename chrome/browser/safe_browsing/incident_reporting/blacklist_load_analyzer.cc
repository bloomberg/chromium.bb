// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/blacklist_load_analyzer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/incident_reporting/add_incident_callback.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"

namespace safe_browsing {

void RegisterBlacklistLoadAnalysis() {
#if defined(OS_WIN)
  scoped_refptr<SafeBrowsingService> safe_browsing_service(
      g_browser_process->safe_browsing_service());

  safe_browsing_service->RegisterDelayedAnalysisCallback(
      base::Bind(&VerifyBlacklistLoadState));
#endif
}

#if !defined(OS_WIN)
void VerifyBlacklistLoadState(const AddIncidentCallback& callback) {
}

bool GetLoadedBlacklistedModules(std::vector<base::string16>* module_names) {
  return false;
}
#endif  // !defined(OS_WIN)

}  // namespace safe_browsing
