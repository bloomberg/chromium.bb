// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/variations_seed_signature_incident.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

scoped_ptr<Incident> MakeIncident(bool alternate) {
  scoped_ptr<ClientIncidentReport_IncidentData_VariationsSeedSignatureIncident>
      incident(
          new ClientIncidentReport_IncidentData_VariationsSeedSignatureIncident);
  if (alternate) {
    incident->set_variations_seed_signature(
        "AEUCID+QmAfajh/kk4zZyv0IUisZ84sIddnjiW9yAXjFJIMFAiEAtVUHhFA/4M6Bff2Gaz"
        "L7tXVLhURxUQcpiMg9eMLWO0U=");
  } else {
    incident->set_variations_seed_signature(
        "MEUCID+QmAfajh/kk4zZyv0IUisZ84sIddnjiW9yAXjFJIMFAiEAtVUHhFA/4M6Bff2Gaz"
        "L7tXVLhURxUQcpiMg9eMLWO0U=");
  }
  return make_scoped_ptr(new VariationsSeedSignatureIncident(incident.Pass()));
}

}  // namespace

TEST(VariationsSeedSignatureIncident, GetType) {
  ASSERT_EQ(IncidentType::VARIATIONS_SEED_SIGNATURE,
            MakeIncident(false)->GetType());
}

// Tests that GetKey returns a constant.
TEST(VariationsSeedSignatureIncident, KeyIsConstant) {
  EXPECT_EQ(std::string("variations_seed_signature"),
            MakeIncident(false)->GetKey());
  EXPECT_EQ(std::string("variations_seed_signature"),
            MakeIncident(true)->GetKey());
}

// Tests that GetDigest returns the same value for the same incident.
TEST(VariationsSeedSignatureIncident, SameIncidentSameDigest) {
  EXPECT_EQ(MakeIncident(false)->ComputeDigest(),
            MakeIncident(false)->ComputeDigest());
}

// Tests that GetDigest returns different values for different incidents.
TEST(VariationsSeedSignatureIncident, DifferentIncidentDifferentDigest) {
  EXPECT_NE(MakeIncident(false)->ComputeDigest(),
            MakeIncident(true)->ComputeDigest());
}

}  // namespace safe_browsing
