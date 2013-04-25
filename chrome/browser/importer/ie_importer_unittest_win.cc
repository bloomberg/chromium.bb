// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <windows.h>

#include <vector>

#include "base/string16.h"
#include "components/webdata/encryptor/ie7_password.h"

TEST(IEImporterTest, IE7Importer) {
  // This is the unencrypted values of my keys under Storage2.
  // The passwords have been manually changed to abcdef... but the size remains
  // the same.
  unsigned char data1[] = "\x0c\x00\x00\x00\x38\x00\x00\x00\x2c\x00\x00\x00"
                          "\x57\x49\x43\x4b\x18\x00\x00\x00\x02\x00\x00\x00"
                          "\x67\x00\x72\x00\x01\x00\x00\x00\x00\x00\x00\x00"
                          "\x00\x00\x00\x00\x4e\xfa\x67\x76\x22\x94\xc8\x01"
                          "\x08\x00\x00\x00\x12\x00\x00\x00\x4e\xfa\x67\x76"
                          "\x22\x94\xc8\x01\x0c\x00\x00\x00\x61\x00\x62\x00"
                          "\x63\x00\x64\x00\x65\x00\x66\x00\x67\x00\x68\x00"
                          "\x00\x00\x61\x00\x62\x00\x63\x00\x64\x00\x65\x00"
                          "\x66\x00\x67\x00\x68\x00\x69\x00\x6a\x00\x6b\x00"
                          "\x6c\x00\x00\x00";

  unsigned char data2[] = "\x0c\x00\x00\x00\x38\x00\x00\x00\x24\x00\x00\x00"
                          "\x57\x49\x43\x4b\x18\x00\x00\x00\x02\x00\x00\x00"
                          "\x67\x00\x72\x00\x01\x00\x00\x00\x00\x00\x00\x00"
                          "\x00\x00\x00\x00\xa8\xea\xf4\xe5\x9f\x9a\xc8\x01"
                          "\x09\x00\x00\x00\x14\x00\x00\x00\xa8\xea\xf4\xe5"
                          "\x9f\x9a\xc8\x01\x07\x00\x00\x00\x61\x00\x62\x00"
                          "\x63\x00\x64\x00\x65\x00\x66\x00\x67\x00\x68\x00"
                          "\x69\x00\x00\x00\x61\x00\x62\x00\x63\x00\x64\x00"
                          "\x65\x00\x66\x00\x67\x00\x00\x00";



  std::vector<unsigned char> decrypted_data1;
  decrypted_data1.resize(arraysize(data1));
  memcpy(&decrypted_data1.front(), data1, sizeof(data1));

  std::vector<unsigned char> decrypted_data2;
  decrypted_data2.resize(arraysize(data2));
  memcpy(&decrypted_data2.front(), data2, sizeof(data2));

  string16 password;
  string16 username;
  ASSERT_TRUE(ie7_password::GetUserPassFromData(decrypted_data1, &username,
                                                &password));
  EXPECT_EQ(L"abcdefgh", username);
  EXPECT_EQ(L"abcdefghijkl", password);

  ASSERT_TRUE(ie7_password::GetUserPassFromData(decrypted_data2, &username,
                                                &password));
  EXPECT_EQ(L"abcdefghi", username);
  EXPECT_EQ(L"abcdefg", password);
}
