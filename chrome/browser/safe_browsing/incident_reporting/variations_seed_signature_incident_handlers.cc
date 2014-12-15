// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/variations_seed_signature_incident_handlers.h"

#include "base/logging.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_handler_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

std::string GetVariationsSeedSignatureIncidentKey(
    const ClientIncidentReport_IncidentData& incident_data) {
  DCHECK(incident_data.has_variations_seed_signature());
  return "variations_seed_signature";
}

uint32_t GetVariationsSeedSignatureIncidentDigest(
    const ClientIncidentReport_IncidentData& incident_data) {
  DCHECK(incident_data.has_variations_seed_signature());
  return HashMessage(incident_data.variations_seed_signature());
}

}  // namespace safe_browsing
