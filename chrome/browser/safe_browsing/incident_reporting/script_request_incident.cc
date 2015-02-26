// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/script_request_incident.h"

#include "base/logging.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_handler_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

ScriptRequestIncident::ScriptRequestIncident(
    scoped_ptr<ClientIncidentReport_IncidentData_ScriptRequestIncident>
        script_request_incident) {
  DCHECK(script_request_incident);
  DCHECK(script_request_incident->has_script_digest());
  payload()->set_allocated_script_request(script_request_incident.release());
}

ScriptRequestIncident::~ScriptRequestIncident() {
}

IncidentType ScriptRequestIncident::GetType() const {
  return IncidentType::SCRIPT_REQUEST;
}

std::string ScriptRequestIncident::GetKey() const {
  // Use a static key in addition to a fixed digest below to ensure that only
  // one incident per user is reported.
  return "script_request_incident";
}

uint32_t ScriptRequestIncident::ComputeDigest() const {
  // Return a constant in addition to a fixed key above to ensure that only one
  // incident per user is reported.
  return 42;
}

}  // namespace safe_browsing
