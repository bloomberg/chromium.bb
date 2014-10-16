// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_VARIATIONS_SEED_SIGNATURE_ANALYZER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_VARIATIONS_SEED_SIGNATURE_ANALYZER_H_

#include "base/strings/string16.h"
#include "chrome/browser/safe_browsing/incident_reporting/add_incident_callback.h"

namespace safe_browsing {

// Registers a process-wide analysis with the incident reporting service that
// will verify the variations seed signature.
void RegisterVariationsSeedSignatureAnalysis();

// Callback to pass to the incident reporting service. The incident reporting
// service will verify if the variations seed signature is invalid.
void VerifyVariationsSeedSignature(const AddIncidentCallback& callback);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_VARIATIONS_SEED_SIGNATURE_ANALYZER_H_
