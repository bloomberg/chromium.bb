// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_field_trial.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DownloadFieldTrialTest, FinchStrings) {
  const std::string malicious_condition("Condition3Malicious");
  EXPECT_EQ(ASCIIToUTF16("This file is malicious."),
            AssembleMalwareFinchString(malicious_condition, base::string16()));
  EXPECT_EQ(ASCIIToUTF16("File 'malware.exe' is malicious."),
            AssembleMalwareFinchString(malicious_condition,
                                       ASCIIToUTF16("malware.exe")));
}
