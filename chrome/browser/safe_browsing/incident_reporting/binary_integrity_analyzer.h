// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_H_

#include <vector>

#include "chrome/browser/safe_browsing/incident_reporting/add_incident_callback.h"

namespace base {
class FilePath;
}  // namespace base

namespace safe_browsing {

// Registers a process-wide analysis with the incident reporting service that
// will verify the signature of the most critical binaries used by Chrome. It
// will send an incident report every time a signature verification fails.
void RegisterBinaryIntegrityAnalysis();

// Callback to pass to the incident reporting service. The incident reporting
// service will decide when to start the analysis.
void VerifyBinaryIntegrity(const AddIncidentCallback& callback);

// Returns a vector containing the paths to all the binaries to verify.
std::vector<base::FilePath> GetCriticalBinariesPath();

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_H_
