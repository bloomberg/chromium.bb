// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/omnibox_incident_handlers.h"

#include "base/logging.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

std::string GetOmniboxIncidentKey(
    const ClientIncidentReport_IncidentData& incident_data) {
  DCHECK(incident_data.has_omnibox_interaction());
  // Return a constant, as only one kind of suspicious interaction is detected.
  return std::string("1");
}

uint32_t GetOmniboxIncidentDigest(
    const ClientIncidentReport_IncidentData& incident_data) {
  DCHECK(incident_data.has_omnibox_interaction());
  // Return a constant (independent of the incident's payload) so that only the
  // first suspicious interaction is reported.
  return 1u;
}

}  // namespace safe_browsing
