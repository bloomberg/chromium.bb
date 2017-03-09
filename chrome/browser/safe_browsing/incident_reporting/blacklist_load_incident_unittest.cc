// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/blacklist_load_incident.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

std::unique_ptr<Incident> MakeIncident(const char* path) {
  std::unique_ptr<ClientIncidentReport_IncidentData_BlacklistLoadIncident>
      incident(new ClientIncidentReport_IncidentData_BlacklistLoadIncident);
  incident->set_path(path);
  return base::MakeUnique<BlacklistLoadIncident>(std::move(incident));
}

}  // namespace

TEST(BlacklistLoadIncident, GetType) {
  ASSERT_EQ(IncidentType::BLACKLIST_LOAD, MakeIncident("foo")->GetType());
}

// Tests that GetKey returns the dll path.
TEST(BlacklistLoadIncident, KeyIsPath) {
  ASSERT_EQ(std::string("foo"), MakeIncident("foo")->GetKey());
}

// Tests that GetDigest returns the same value for the same incident.
TEST(BlacklistLoadIncident, SameIncidentSameDigest) {
  ASSERT_EQ(MakeIncident("foo")->ComputeDigest(),
            MakeIncident("foo")->ComputeDigest());
}

// Tests that GetDigest returns different values for different incidents.
TEST(BlacklistLoadIncident, DifferentIncidentDifferentDigest) {
  ASSERT_NE(MakeIncident("foo")->ComputeDigest(),
            MakeIncident("bar")->ComputeDigest());
}

}  // namespace safe_browsing
