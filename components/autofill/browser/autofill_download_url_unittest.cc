// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/autofill_download_url.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StartsWith;

TEST(AutofillDownloadUrlTest, CheckDefaultUrls) {
  std::string query_url =
      autofill::GetAutofillQueryUrl().spec();
  EXPECT_THAT(query_url,
      StartsWith("https://clients1.google.com/tbproxy/af/query?client="));

  std::string upload_url =
      autofill::GetAutofillUploadUrl().spec();
  EXPECT_THAT(upload_url,
      StartsWith("https://clients1.google.com/tbproxy/af/upload?client="));
}

