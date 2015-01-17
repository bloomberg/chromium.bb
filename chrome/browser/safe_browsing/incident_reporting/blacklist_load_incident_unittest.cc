// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/blacklist_load_incident.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

scoped_ptr<Incident> MakeIncident(const char* digest) {
  scoped_ptr<ClientIncidentReport_IncidentData_BlacklistLoadIncident> incident(
      new ClientIncidentReport_IncidentData_BlacklistLoadIncident);
  incident->set_path("foo");
  return make_scoped_ptr(new BlacklistLoadIncident(incident.Pass()));
}

}  // namespace

TEST(BlacklistLoadIncident, GetType) {
  ASSERT_EQ(IncidentType::BLACKLIST_LOAD, MakeIncident("37")->GetType());
}

// Tests that GetKey returns the dll path.
TEST(BlacklistLoadIncident, KeyIsPath) {
  ASSERT_EQ(std::string("foo"), MakeIncident("37")->GetKey());
}

// Tests that GetDigest returns the same value for the same incident.
TEST(BlacklistLoadIncident, SameIncidentSameDigest) {
  ASSERT_EQ(MakeIncident("37")->ComputeDigest(),
            MakeIncident("37")->ComputeDigest());
}

// Tests that GetDigest returns different values for different incidents.
TEST(BlacklistLoadIncident, DifferentIncidentDifferentDigest) {
  ASSERT_EQ(MakeIncident("37")->ComputeDigest(),
            MakeIncident("42")->ComputeDigest());
}

}  // namespace safe_browsing
