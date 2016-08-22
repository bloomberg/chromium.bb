// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/variations_seed_signature_analyzer.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/variations_seed_signature_incident.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

void VerifyVariationsSeedSignatureOnUIThread(
    std::unique_ptr<IncidentReceiver> incident_receiver) {
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (!variations_service)
    return;
  std::string invalid_signature =
      variations_service->GetInvalidVariationsSeedSignature();
  if (!invalid_signature.empty()) {
    std::unique_ptr<
        ClientIncidentReport_IncidentData_VariationsSeedSignatureIncident>
        variations_seed_signature(
            new ClientIncidentReport_IncidentData_VariationsSeedSignatureIncident());
    variations_seed_signature->set_variations_seed_signature(invalid_signature);
    incident_receiver->AddIncidentForProcess(
        base::MakeUnique<VariationsSeedSignatureIncident>(
            std::move(variations_seed_signature)));
  }
}

}  // namespace

void RegisterVariationsSeedSignatureAnalysis() {
  scoped_refptr<SafeBrowsingService> safe_browsing_service(
      g_browser_process->safe_browsing_service());

  safe_browsing_service->RegisterDelayedAnalysisCallback(
      base::Bind(&VerifyVariationsSeedSignature));
}

void VerifyVariationsSeedSignature(
    std::unique_ptr<IncidentReceiver> incident_receiver) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&VerifyVariationsSeedSignatureOnUIThread,
                 base::Passed(&incident_receiver)));
}

}  // namespace safe_browsing
