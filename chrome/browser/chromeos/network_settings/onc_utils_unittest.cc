// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_settings/onc_utils.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/network_settings/onc_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

TEST(ONCDecrypterTest, BrokenEncryptionIterations) {
  scoped_ptr<base::DictionaryValue> encrypted_onc =
      test_utils::ReadTestDictionary("broken-encrypted-iterations.onc");

  std::string error;
  scoped_ptr<base::DictionaryValue> decrypted_onc =
      Decrypt("test0000", *encrypted_onc, &error);

  EXPECT_EQ(NULL, decrypted_onc.get());
  EXPECT_FALSE(error.empty());
}

TEST(ONCDecrypterTest, BrokenEncryptionZeroIterations) {
  scoped_ptr<base::DictionaryValue> encrypted_onc =
      test_utils::ReadTestDictionary("broken-encrypted-zero-iterations.onc");

  std::string error;
  scoped_ptr<base::DictionaryValue> decrypted_onc =
      Decrypt("test0000", *encrypted_onc, &error);

  EXPECT_EQ(NULL, decrypted_onc.get());
  EXPECT_FALSE(error.empty());
}

TEST(ONCDecrypterTest, LoadEncryptedOnc) {
  scoped_ptr<base::DictionaryValue> encrypted_onc =
      test_utils::ReadTestDictionary("encrypted.onc");
  scoped_ptr<base::DictionaryValue> expected_decrypted_onc =
      test_utils::ReadTestDictionary("decrypted.onc");

  std::string error;
  scoped_ptr<base::DictionaryValue> actual_decrypted_onc =
      Decrypt("test0000", *encrypted_onc, &error);

  base::DictionaryValue emptyDict;
  EXPECT_TRUE(test_utils::Equals(expected_decrypted_onc.get(),
                                 actual_decrypted_onc.get()));
  EXPECT_TRUE(error.empty()) << error;
}

}  // namespace onc
}  // namespace chromeos
