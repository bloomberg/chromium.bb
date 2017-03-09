// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/suspicious_module_incident.h"

#include "base/logging.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_handler_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

SuspiciousModuleIncident::SuspiciousModuleIncident(
    std::unique_ptr<ClientIncidentReport_IncidentData_SuspiciousModuleIncident>
        suspicious_module_incident) {
  DCHECK(suspicious_module_incident);
  DCHECK(suspicious_module_incident->has_path());
  payload()->set_allocated_suspicious_module(
      suspicious_module_incident.release());
}

SuspiciousModuleIncident::~SuspiciousModuleIncident() {}

IncidentType SuspiciousModuleIncident::GetType() const {
  return IncidentType::SUSPICIOUS_MODULE;
}

// Returns the sanitized path of the module.
std::string SuspiciousModuleIncident::GetKey() const {
  return payload()->suspicious_module().path();
}

// Returns a digest computed over the payload.
uint32_t SuspiciousModuleIncident::ComputeDigest() const {
  return HashMessage(payload()->suspicious_module());
}

}  // namespace safe_browsing
