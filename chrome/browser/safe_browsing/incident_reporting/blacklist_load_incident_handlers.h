// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BLACKLIST_LOAD_INCIDENT_HANDLERS_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BLACKLIST_LOAD_INCIDENT_HANDLERS_H_

#include <stdint.h>

#include <string>

namespace safe_browsing {

class ClientIncidentReport_IncidentData;

// Returns the path of the loaded blacklisted module.
std::string GetBlacklistLoadIncidentKey(
    const ClientIncidentReport_IncidentData& incident_data);

// Returns a digest of the loaded blacklisted module.
uint32_t GetBlacklistLoadIncidentDigest(
    const ClientIncidentReport_IncidentData& incident_data);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BLACKLIST_LOAD_INCIDENT_HANDLERS_H_
