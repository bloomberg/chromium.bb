// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_SUSPICIOUS_MODULE_INCIDENT_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_SUSPICIOUS_MODULE_INCIDENT_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"

namespace safe_browsing {

class ClientIncidentReport_IncidentData_SuspiciousModuleIncident;

// An incident representing a module loaded in Chrome that wasn't in the module
// whitelist.
class SuspiciousModuleIncident : public Incident {
 public:
  explicit SuspiciousModuleIncident(
      std::unique_ptr<
          ClientIncidentReport_IncidentData_SuspiciousModuleIncident>
          suspicious_modules);
  ~SuspiciousModuleIncident() override;

  // Incident methods:
  IncidentType GetType() const override;
  std::string GetKey() const override;
  uint32_t ComputeDigest() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SuspiciousModuleIncident);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_SUSPICIOUS_MODULE_INCIDENT_H_
