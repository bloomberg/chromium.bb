// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/blacklist_load_incident_handlers.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

scoped_ptr<safe_browsing::ClientIncidentReport_IncidentData> MakeIncident() {
  scoped_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      new safe_browsing::ClientIncidentReport_IncidentData);
  incident->mutable_blacklist_load()->set_path("foo");
  return incident.Pass();
}

}  // namespace

// Tests that GetKey returns the dll path.
TEST(GetBlacklistLoadIncidentKey, KeyIsPath) {
  safe_browsing::ClientIncidentReport_IncidentData incident;

  incident.mutable_blacklist_load()->set_path("foo");
  ASSERT_EQ(std::string("foo"),
            safe_browsing::GetBlacklistLoadIncidentKey(incident));
}

// Tests that GetDigest returns the same value for the same incident.
TEST(GetBlacklistLoadIncidentDigest, SameIncidentSameDigest) {
  scoped_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      MakeIncident());

  uint32_t digest = safe_browsing::GetBlacklistLoadIncidentDigest(*incident);
  ASSERT_EQ(digest,
            safe_browsing::GetBlacklistLoadIncidentDigest(*MakeIncident()));
}

// Tests that GetDigest returns different values for different incidents.
TEST(GetBlacklistLoadIncidentDigest, DifferentIncidentDifferentDigest) {
  scoped_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      MakeIncident());
  scoped_ptr<safe_browsing::ClientDownloadRequest_Digests> digest_proto(
      new safe_browsing::ClientDownloadRequest_Digests);
  digest_proto->set_sha256("37");
  incident->mutable_blacklist_load()->set_allocated_digest(
      digest_proto.release());

  scoped_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident2(
      MakeIncident());
  scoped_ptr<safe_browsing::ClientDownloadRequest_Digests> digest_proto2(
      new safe_browsing::ClientDownloadRequest_Digests);
  digest_proto2->set_sha256("42");
  incident2->mutable_blacklist_load()->set_allocated_digest(
      digest_proto2.release());

  ASSERT_NE(safe_browsing::GetBlacklistLoadIncidentDigest(*incident),
            safe_browsing::GetBlacklistLoadIncidentDigest(*incident2));
}
