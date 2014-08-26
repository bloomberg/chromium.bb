// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/blacklist_load_incident_handlers.h"

#include "base/logging.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_handler_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

std::string GetBlacklistLoadIncidentKey(
    const ClientIncidentReport_IncidentData& incident_data) {
  DCHECK(incident_data.has_blacklist_load());
  DCHECK(incident_data.blacklist_load().has_path());
  return incident_data.blacklist_load().path();
}

uint32_t GetBlacklistLoadIncidentDigest(
    const ClientIncidentReport_IncidentData& incident_data) {
  DCHECK(incident_data.has_blacklist_load());
  return HashMessage(incident_data.blacklist_load());
}

}  // namespace safe_browsing
