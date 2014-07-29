// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tracked_preference_incident_handlers.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

scoped_ptr<safe_browsing::ClientIncidentReport_IncidentData> MakeIncident() {
  scoped_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      new safe_browsing::ClientIncidentReport_IncidentData);

  incident->mutable_tracked_preference()->set_path("foo");
  incident->mutable_tracked_preference()->set_atomic_value("bar");
  incident->mutable_tracked_preference()->set_value_state(
      safe_browsing::
          ClientIncidentReport_IncidentData_TrackedPreferenceIncident_ValueState_CLEARED);
  return incident.Pass();
}

}  // namespace

// Tests that GetKey returns the preference path.
TEST(GetTrackedPreferenceIncidentKey, KeyIsPath) {
  safe_browsing::ClientIncidentReport_IncidentData incident;

  incident.mutable_tracked_preference()->set_path("foo");
  ASSERT_EQ(std::string("foo"),
            safe_browsing::GetTrackedPreferenceIncidentKey(incident));
}

// Tests that GetDigest returns the same value for the same incident.
TEST(GetTrackedPreferenceIncidentDigest, SameIncidentSameDigest) {
  scoped_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      MakeIncident());

  uint32_t digest =
      safe_browsing::GetTrackedPreferenceIncidentDigest(*incident);
  ASSERT_EQ(digest,
            safe_browsing::GetTrackedPreferenceIncidentDigest(*MakeIncident()));
}

// Tests that GetDigest returns the same value for the same incident.
TEST(GetTrackedPreferenceIncidentDigest, DifferentIncidentDifferentDigest) {
  scoped_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      MakeIncident());

  uint32_t digest =
      safe_browsing::GetTrackedPreferenceIncidentDigest(*incident);

  scoped_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident2(
      MakeIncident());
  incident2->mutable_tracked_preference()->set_value_state(
      safe_browsing::
          ClientIncidentReport_IncidentData_TrackedPreferenceIncident_ValueState_CHANGED);

  ASSERT_NE(digest,
            safe_browsing::GetTrackedPreferenceIncidentDigest(*incident2));
}
