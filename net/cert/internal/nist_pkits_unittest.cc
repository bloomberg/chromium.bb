// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/nist_pkits_unittest.h"

#include "base/strings/string_split.h"
#include "net/cert/internal/certificate_policies.h"

namespace net {

namespace {

// 2.16.840.1.101.3.2.1.48.1
const uint8_t kTestPolicy1[] = {0x60, 0x86, 0x48, 0x01, 0x65,
                                0x03, 0x02, 0x01, 0x30, 0x01};

// 2.16.840.1.101.3.2.1.48.2
const uint8_t kTestPolicy2[] = {0x60, 0x86, 0x48, 0x01, 0x65,
                                0x03, 0x02, 0x01, 0x30, 0x02};

// 2.16.840.1.101.3.2.1.48.3
const uint8_t kTestPolicy3[] = {0x60, 0x86, 0x48, 0x01, 0x65,
                                0x03, 0x02, 0x01, 0x30, 0x03};

// 2.16.840.1.101.3.2.1.48.6
const uint8_t kTestPolicy6[] = {0x60, 0x86, 0x48, 0x01, 0x65,
                                0x03, 0x02, 0x01, 0x30, 0x06};

}  // namespace

PkitsTestSettings::PkitsTestSettings() {
  SetInitialPolicySet("anyPolicy");
}

PkitsTestSettings::~PkitsTestSettings() = default;

void PkitsTestSettings::SetInitialPolicySet(const char* const policy_names) {
  initial_policy_set.clear();
  std::vector<std::string> names = base::SplitString(
      policy_names, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& policy_name : names) {
    if (policy_name == "anyPolicy") {
      initial_policy_set.insert(AnyPolicy());
    } else if (policy_name == "NIST-test-policy-1") {
      initial_policy_set.insert(der::Input(kTestPolicy1));
    } else if (policy_name == "NIST-test-policy-2") {
      initial_policy_set.insert(der::Input(kTestPolicy2));
    } else if (policy_name == "NIST-test-policy-3") {
      initial_policy_set.insert(der::Input(kTestPolicy3));
    } else if (policy_name == "NIST-test-policy-6") {
      initial_policy_set.insert(der::Input(kTestPolicy6));
    } else {
      ADD_FAILURE() << "Unknown policy name: " << policy_name;
    }
  }
}

}  // namespace net
