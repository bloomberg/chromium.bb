// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omaha_query_params/omaha_query_params.h"

#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPrintf;

namespace chrome {

bool Contains(const std::string& source, const std::string& target) {
  return source.find(target) != std::string::npos;
}

void TestParams(OmahaQueryParams::ProdId prod_id) {
  std::string params = OmahaQueryParams::Get(prod_id);

  // This doesn't so much test what the values are (since that would be an
  // almost exact duplication of code with omaha_query_params.cc, and wouldn't
  // really test anything) as it is a verification that all the params are
  // present in the generated string.
  EXPECT_TRUE(
      Contains(params, StringPrintf("os=%s", OmahaQueryParams::GetOS())));
  EXPECT_TRUE(
      Contains(params, StringPrintf("arch=%s", OmahaQueryParams::GetArch())));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf("prod=%s", OmahaQueryParams::GetProdIdString(prod_id))));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf("prodchannel=%s", OmahaQueryParams::GetChannelString())));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf("prodversion=%s", chrome::VersionInfo().Version().c_str())));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf("lang=%s", OmahaQueryParams::GetLang())));
}

TEST(OmahaQueryParams, GetOmahaQueryParams) {
  TestParams(OmahaQueryParams::CRX);
  TestParams(OmahaQueryParams::CHROME);
}

}  // namespace chrome
