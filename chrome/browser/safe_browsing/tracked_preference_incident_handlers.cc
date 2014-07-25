// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tracked_preference_incident_handlers.h"

#include "base/logging.h"
#include "chrome/browser/safe_browsing/incident_handler_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

std::string GetTrackedPreferenceIncidentKey(
    const ClientIncidentReport_IncidentData& incident_data) {
  DCHECK(incident_data.has_tracked_preference());
  DCHECK(incident_data.tracked_preference().has_path());
  return incident_data.tracked_preference().path();
}

uint32_t GetTrackedPreferenceIncidentDigest(
    const ClientIncidentReport_IncidentData& incident_data) {
  DCHECK(incident_data.has_tracked_preference());

  // Tracked preference incidents are sufficiently canonical (and have no
  // default values), so it's safe to serialize the incident as given.
  return HashMessage(incident_data.tracked_preference());
}

}  // namespace safe_browsing
