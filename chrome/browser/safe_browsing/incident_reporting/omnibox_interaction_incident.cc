// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/omnibox_interaction_incident.h"

#include "base/logging.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

OmniboxInteractionIncident::OmniboxInteractionIncident(
    scoped_ptr<ClientIncidentReport_IncidentData_OmniboxInteractionIncident>
        omnibox_interaction_incident) {
  DCHECK(omnibox_interaction_incident);
  DCHECK(omnibox_interaction_incident->has_origin());
  payload()->set_allocated_omnibox_interaction(
      omnibox_interaction_incident.release());
}

OmniboxInteractionIncident::~OmniboxInteractionIncident() {
}

IncidentType OmniboxInteractionIncident::GetType() const {
  return IncidentType::OMNIBOX_INTERACTION;
}

std::string OmniboxInteractionIncident::GetKey() const {
  // Return a constant, as only one kind of suspicious interaction is detected.
  return "1";
}

uint32_t OmniboxInteractionIncident::ComputeDigest() const {
  // Return a constant (independent of the incident's payload) so that only the
  // first suspicious interaction is reported.
  return 1u;
}

}  // namespace safe_browsing
