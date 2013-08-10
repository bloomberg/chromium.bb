// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_util.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DownloadUtilTest, FinchStrings) {
  EXPECT_EQ(
      ASCIIToUTF16("This file is malicious."),
      download_util::AssembleMalwareFinchString(
          download_util::kCondition3Malicious, string16()));
  EXPECT_EQ(
      ASCIIToUTF16("File 'malware.exe' is malicious."),
      download_util::AssembleMalwareFinchString(
          download_util::kCondition3Malicious, ASCIIToUTF16("malware.exe")));
}
