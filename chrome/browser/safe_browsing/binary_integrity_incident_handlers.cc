// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/binary_integrity_incident_handlers.h"

#include "base/logging.h"
#include "chrome/browser/safe_browsing/incident_handler_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {

std::string GetBinaryIntegrityIncidentKey(
    const ClientIncidentReport_IncidentData& incident_data) {
  DCHECK(incident_data.has_binary_integrity());
  DCHECK(incident_data.binary_integrity().has_file_basename());
  return incident_data.binary_integrity().file_basename();
}

uint32_t GetBinaryIntegrityIncidentDigest(
    const ClientIncidentReport_IncidentData& incident_data) {
  DCHECK(incident_data.has_binary_integrity());
  return HashMessage(incident_data.binary_integrity());
}

}  // namespace safe_browsing
