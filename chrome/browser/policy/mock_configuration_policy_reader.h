// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_READER_H_
#define CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_READER_H_
#pragma once

#include "chrome/browser/policy/configuration_policy_reader.h"

#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

// Mock ConfigurationPolicyReader implementation.
class MockConfigurationPolicyReader : public ConfigurationPolicyReader {
 public:
  MockConfigurationPolicyReader();
  virtual ~MockConfigurationPolicyReader();

  MOCK_CONST_METHOD1(GetPolicyStatus,
      DictionaryValue*(ConfigurationPolicyType policy));
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_CONFIGURATION_POLICY_READER_H_
