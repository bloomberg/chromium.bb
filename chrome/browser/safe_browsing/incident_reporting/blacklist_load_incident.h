// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BLACKLIST_LOAD_INCIDENT_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BLACKLIST_LOAD_INCIDENT_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"

namespace safe_browsing {

class ClientIncidentReport_IncidentData_BlacklistLoadIncident;

// An incident representing a module that evades the browser DLL blacklist.
class BlacklistLoadIncident : public Incident {
 public:
  explicit BlacklistLoadIncident(scoped_ptr<
      ClientIncidentReport_IncidentData_BlacklistLoadIncident> blacklist_load);
  ~BlacklistLoadIncident() override;

  // Incident methods:
  IncidentType GetType() const override;
  std::string GetKey() const override;
  uint32_t ComputeDigest() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlacklistLoadIncident);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BLACKLIST_LOAD_INCIDENT_H_
