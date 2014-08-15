// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_incident_handlers.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

scoped_ptr<ClientIncidentReport_IncidentData> MakeIncident() {
  scoped_ptr<ClientIncidentReport_IncidentData> incident(
      new ClientIncidentReport_IncidentData);

  incident->mutable_binary_integrity()->set_file_basename("foo");

  // Set the signature.
  incident->mutable_binary_integrity()->mutable_signature()->set_trusted(true);
  ClientDownloadRequest_CertificateChain* certificate_chain =
      incident->mutable_binary_integrity()->mutable_signature()
          ->add_certificate_chain();

  // Fill the certificate chain with 2 elements.
  const unsigned char certificates[][5] = {
      {42, 255, 100, 53, 2},
      {64, 33, 51, 91, 210},
  };
  for (size_t i = 0; i < arraysize(certificates); ++i) {
    ClientDownloadRequest_CertificateChain_Element* element =
        certificate_chain->add_element();
    element->set_certificate(certificates[i], arraysize(certificates[i]));
  }

  return incident.Pass();
}

}  // namespace

TEST(BinaryIntegrityIncidentHandlersTest, GetKeyIsFile) {
  ClientIncidentReport_IncidentData incident;

  incident.mutable_binary_integrity()->set_file_basename("foo");
  ASSERT_EQ(std::string("foo"), GetBinaryIntegrityIncidentKey(incident));
}

TEST(BinaryIntegrityIncidentHandlersTest, SameIncidentSameDigest) {
  scoped_ptr<ClientIncidentReport_IncidentData> incident(MakeIncident());

  uint32_t digest = GetBinaryIntegrityIncidentDigest(*incident);
  ASSERT_EQ(digest, GetBinaryIntegrityIncidentDigest(*MakeIncident()));
}

TEST(BinaryIntegrityIncidentHandlersTest, DifferentIncidentDifferentDigest) {
  scoped_ptr<ClientIncidentReport_IncidentData> incident(MakeIncident());

  uint32_t digest = GetBinaryIntegrityIncidentDigest(*incident);

  scoped_ptr<ClientIncidentReport_IncidentData> incident2(MakeIncident());
  incident2->mutable_binary_integrity()->set_file_basename("bar");

  ASSERT_NE(digest, GetBinaryIntegrityIncidentDigest(*incident2));
}

}  // namespace safe_browsing
