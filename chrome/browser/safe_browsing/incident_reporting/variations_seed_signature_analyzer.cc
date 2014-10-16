// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/variations_seed_signature_analyzer.h"

#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

void RegisterVariationsSeedSignatureAnalysis() {
  scoped_refptr<SafeBrowsingService> safe_browsing_service(
      g_browser_process->safe_browsing_service());

  safe_browsing_service->RegisterDelayedAnalysisCallback(
      base::Bind(&VerifyVariationsSeedSignature));
}

void VerifyVariationsSeedSignature(const AddIncidentCallback& callback) {
  if (g_browser_process->variations_service() == nullptr)
    return;
  std::string invalid_signature = g_browser_process->variations_service()
                                      ->GetInvalidVariationsSeedSignature();
  if (!invalid_signature.empty()) {
    scoped_ptr<ClientIncidentReport_IncidentData> incident_data(
        new ClientIncidentReport_IncidentData());
    ClientIncidentReport_IncidentData_VariationsSeedSignatureIncident*
        variations_seed_signature =
            incident_data->mutable_variations_seed_signature();
    variations_seed_signature->set_variations_seed_signature(invalid_signature);
    callback.Run(incident_data.Pass());
  }
}

}  // namespace safe_browsing
