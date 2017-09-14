// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/general_names.h"

#include "net/cert/internal/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

::testing::AssertionResult LoadTestData(const char* token,
                                        const std::string& basename,
                                        std::string* result) {
  std::string path = "net/data/name_constraints_unittest/" + basename;

  const PemBlockMapping mappings[] = {
      {token, result},
  };

  return ReadTestDataFromPemFile(path, mappings);
}

::testing::AssertionResult LoadTestSubjectAltNameData(
    const std::string& basename,
    std::string* result) {
  return LoadTestData("SUBJECT ALTERNATIVE NAME", basename, result);
}

}  // namespace

TEST(GeneralNames, CreateFailsOnEmptySubjectAltName) {
  std::string invalid_san_der;
  ASSERT_TRUE(
      LoadTestSubjectAltNameData("san-invalid-empty.pem", &invalid_san_der));
  CertErrors errors;
  EXPECT_FALSE(GeneralNames::Create(der::Input(&invalid_san_der), &errors));
}

TEST(GeneralNames, CreateFailsOnInvalidIpInSubjectAltName) {
  std::string invalid_san_der;
  ASSERT_TRUE(LoadTestSubjectAltNameData("san-invalid-ipaddress.pem",
                                         &invalid_san_der));
  CertErrors errors;
  EXPECT_FALSE(GeneralNames::Create(der::Input(&invalid_san_der), &errors));
}

}  // namespace net
