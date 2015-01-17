// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/variations_seed_signature_incident.h"

#include "base/logging.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_handler_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

VariationsSeedSignatureIncident::VariationsSeedSignatureIncident(
    scoped_ptr<
        ClientIncidentReport_IncidentData_VariationsSeedSignatureIncident>
        variations_seed_signature_incident) {
  DCHECK(variations_seed_signature_incident);
  DCHECK(variations_seed_signature_incident->has_variations_seed_signature());
  payload()->set_allocated_variations_seed_signature(
      variations_seed_signature_incident.release());
}

VariationsSeedSignatureIncident::~VariationsSeedSignatureIncident() {
}

IncidentType VariationsSeedSignatureIncident::GetType() const {
  return IncidentType::VARIATIONS_SEED_SIGNATURE;
}

std::string VariationsSeedSignatureIncident::GetKey() const {
  return "variations_seed_signature";
}

// Returns a digest computed over the payload.
uint32_t VariationsSeedSignatureIncident::ComputeDigest() const {
  return HashMessage(payload()->variations_seed_signature());
}

}  // namespace safe_browsing
