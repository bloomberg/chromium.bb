// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/variations_seed_signature_incident_handlers.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

scoped_ptr<ClientIncidentReport_IncidentData> MakeIncident() {
  scoped_ptr<ClientIncidentReport_IncidentData> incident(
      new ClientIncidentReport_IncidentData);
  incident->mutable_variations_seed_signature()->set_variations_seed_signature(
      "MEUCID+QmAfajh/kk4zZyv0IUisZ84sIddnjiW9yAXjFJIMFAiEAtVUHhFA/4M6Bff2GazL"
      "7tXVLhURxUQcpiMg9eMLWO0U=");
  return incident.Pass();
}

}  // namespace

// Tests that GetKey returns a constant.
TEST(GetVariationsSeedSignatureIncidentKey, KeyIsConstant) {
  ClientIncidentReport_IncidentData incident;

  incident.mutable_variations_seed_signature()->set_variations_seed_signature(
      "MEUCID+QmAfajh/kk4zZyv0IUisZ84sIddnjiW9yAXjFJIMFAiEAtVUHhFA/4M6Bff2GazL"
      "7tXVLhURxUQcpiMg9eMLWO0U=");
  EXPECT_EQ(std::string("variations_seed_signature"),
            GetVariationsSeedSignatureIncidentKey(incident));
  incident.mutable_variations_seed_signature()->set_variations_seed_signature(
      "AEUCID+QmAfajh/kk4zZyv0IUisZ84sIddnjiW9yAXjFJIMFAiEAtVUHhFA/4M6Bff2GazL"
      "7tXVLhURxUQcpiMg9eMLWO0U=");
  EXPECT_EQ("variations_seed_signature",
            GetVariationsSeedSignatureIncidentKey(incident));
}

// Tests that GetDigest returns the same value for the same incident.
TEST(GetVariationsSeedSignatureIncidentDigest, SameIncidentSameDigest) {
  scoped_ptr<ClientIncidentReport_IncidentData> incident(MakeIncident());

  uint32_t digest = GetVariationsSeedSignatureIncidentDigest(*incident);
  EXPECT_EQ(digest, GetVariationsSeedSignatureIncidentDigest(*MakeIncident()));
}

// Tests that GetDigest returns different values for different incidents.
TEST(GetVariationsSeedSignatureIncidentDigest,
     DifferentIncidentDifferentDigest) {
  scoped_ptr<ClientIncidentReport_IncidentData> incident(MakeIncident());
  incident->mutable_variations_seed_signature()->set_variations_seed_signature(
      "MEUCID+QmAfajh/kk4zZyv0IUisZ84sIddnjiW9yAXjFJIMFAiEAtVUHhFA/4M6Bff2GazL"
      "7tXVLhURxUQcpiMg9eMLWO0U=");

  scoped_ptr<ClientIncidentReport_IncidentData> incident2(MakeIncident());
  incident2->mutable_variations_seed_signature()->set_variations_seed_signature(
      "AEUCID+QmAfajh/kk4zZyv0IUisZ84sIddnjiW9yAXjFJIMFAiEAtVUHhFA/4M6Bff2GazL"
      "7tXVLhURxUQcpiMg9eMLWO0U=");

  EXPECT_NE(GetVariationsSeedSignatureIncidentDigest(*incident),
            GetVariationsSeedSignatureIncidentDigest(*incident2));
}

}  // namespace safe_browsing
