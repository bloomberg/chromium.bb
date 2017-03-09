// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/suspicious_module_incident.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

std::unique_ptr<Incident> MakeIncident(const char* path) {
  std::unique_ptr<ClientIncidentReport_IncidentData_SuspiciousModuleIncident>
      incident(new ClientIncidentReport_IncidentData_SuspiciousModuleIncident);
  incident->set_path(path);
  return base::MakeUnique<SuspiciousModuleIncident>(std::move(incident));
}

}  // namespace

TEST(SuspiciousModuleIncident, GetType) {
  ASSERT_EQ(IncidentType::SUSPICIOUS_MODULE, MakeIncident("foo")->GetType());
}

// Tests that GetKey returns the dll path.
TEST(SuspiciousModuleIncident, KeyIsPath) {
  ASSERT_EQ(std::string("foo"), MakeIncident("foo")->GetKey());
}

// Tests that GetDigest returns the same value for the same incident.
TEST(SuspiciousModuleIncident, SameIncidentSameDigest) {
  ASSERT_EQ(MakeIncident("foo")->ComputeDigest(),
            MakeIncident("foo")->ComputeDigest());
}

// Tests that GetDigest returns different values for different incidents.
TEST(SuspiciousModuleIncident, DifferentIncidentDifferentDigest) {
  ASSERT_NE(MakeIncident("foo")->ComputeDigest(),
            MakeIncident("bar")->ComputeDigest());
}

}  // namespace safe_browsing
