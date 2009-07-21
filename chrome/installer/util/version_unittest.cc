// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/version.h"

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

static const installer::Version* kNoVersion = NULL;

TEST(VersionTest, Parse) {
  EXPECT_EQ(installer::Version::GetVersionFromString(L"1"), kNoVersion);
  EXPECT_EQ(installer::Version::GetVersionFromString(L"1.2"), kNoVersion);
  EXPECT_EQ(installer::Version::GetVersionFromString(L"1.2.3"), kNoVersion);
  EXPECT_EQ(installer::Version::GetVersionFromString(L"1.2.3.4.5"), kNoVersion);
  EXPECT_EQ(installer::Version::GetVersionFromString(L"hokum"), kNoVersion);

  scoped_ptr<installer::Version> v1(
      installer::Version::GetVersionFromString(L"1.22.333.4444"));
  EXPECT_EQ(v1->GetString(), L"1.22.333.4444");
}

bool Implies(bool p, bool q) { return  !p | q; }

TEST(VersionTest, Comparison) {
  static const wchar_t* version_strings[] = {
    L"0.0.0.0",
    L"1.0.0.0",
    L"0.1.0.0",
    L"0.0.1.0",
    L"0.0.0.1",
    L"2.0.193.1",
    L"3.0.183.1",
    L"3.0.187.1",
    L"3.0.189.0",
  };

  std::vector<const installer::Version*> versions;

  for (size_t i = 0;  i < _countof(version_strings);  ++i) {
    std::wstring version_string(version_strings[i]);
    const installer::Version* version =
        installer::Version::GetVersionFromString(version_string);
    EXPECT_NE(version, kNoVersion);
    EXPECT_EQ(version_string, version->GetString());
    versions.push_back(version);
  }

  // Compare all N*N pairs and check that IsHigherThan is antireflexive.
  for (size_t i = 0;  i < versions.size();  ++i) {
    const installer::Version* v1 = versions[i];
    for (size_t j = 0;  j < versions.size();  ++j) {
      const installer::Version* v2 = versions[j];
      // Check exactly one of '>', '<', or '=' is true.
      bool higher = v1->IsHigherThan(v2);
      bool lower = v2->IsHigherThan(v1);
      bool equal = v1->GetString() == v2->GetString();
      EXPECT_EQ(1, higher + lower + equal);
    }
  }

  // Compare all N*N*N triples and check that IsHigherThan is a total order.
  for (size_t i = 0;  i < versions.size();  ++i) {
    const installer::Version* v1 = versions[i];
    for (size_t j = 0;  j < versions.size();  ++j) {
      const installer::Version* v2 = versions[j];
      for (size_t k = 0;  k < versions.size();  ++k) {
        const installer::Version* v3 = versions[j];
        EXPECT_TRUE(Implies(v2->IsHigherThan(v1) && v3->IsHigherThan(v2),
                            v3->IsHigherThan(v1)));
      }
    }
  }

  for (size_t i = 0;  i < versions.size();  ++i) {
    delete versions[i];
  }
}
