// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_normalizer.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

// This test case is about validating valid ONC objects.
TEST(ONCNormalizerTest, NormalizeNetworkConfiguration) {
  Normalizer normalizer(true);
  scoped_ptr<const base::DictionaryValue> data(
      test_utils::ReadTestDictionary("settings_with_normalization.json"));

  const base::DictionaryValue* original = NULL;
  const base::DictionaryValue* expected_normalized = NULL;
  data->GetDictionary("ethernet-and-vpn", &original);
  data->GetDictionary("ethernet-and-vpn-normalized", &expected_normalized);

  scoped_ptr<base::DictionaryValue> actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, actual_normalized.get()));
}

}  // namespace onc
}  // namespace chromeos
