// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/blacklist_load_incident.h"

#include "base/logging.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_handler_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

BlacklistLoadIncident::BlacklistLoadIncident(
    scoped_ptr<ClientIncidentReport_IncidentData_BlacklistLoadIncident>
        blacklist_load_incident) {
  DCHECK(blacklist_load_incident);
  DCHECK(blacklist_load_incident->has_path());
  payload()->set_allocated_blacklist_load(
      blacklist_load_incident.release());
}

BlacklistLoadIncident::~BlacklistLoadIncident() {
}

IncidentType BlacklistLoadIncident::GetType() const {
  return IncidentType::BLACKLIST_LOAD;
}

// Returns the sanitized path of the module.
std::string BlacklistLoadIncident::GetKey() const {
  return payload()->blacklist_load().path();
}

// Returns a digest computed over the payload.
uint32_t BlacklistLoadIncident::ComputeDigest() const {
  return HashMessage(payload()->blacklist_load());
}

}  // namespace safe_browsing
