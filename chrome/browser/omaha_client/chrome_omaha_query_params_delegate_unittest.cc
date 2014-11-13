// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/omaha_client/chrome_omaha_query_params_delegate.h"
#include "chrome/common/chrome_version_info.h"
#include "components/omaha_client/omaha_query_params.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPrintf;

namespace {

bool Contains(const std::string& source, const std::string& target) {
  return source.find(target) != std::string::npos;
}

}  // namespace

void TestParams(omaha_client::OmahaQueryParams::ProdId prod_id) {
  std::string params = omaha_client::OmahaQueryParams::Get(prod_id);

  EXPECT_TRUE(Contains(
      params,
      StringPrintf("os=%s", omaha_client::OmahaQueryParams::GetOS())));
  EXPECT_TRUE(
      Contains(params,
               StringPrintf("arch=%s",
                            omaha_client::OmahaQueryParams::GetArch())));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf(
          "prod=%s",
          omaha_client::OmahaQueryParams::GetProdIdString(prod_id))));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf("prodchannel=%s",
                   ChromeOmahaQueryParamsDelegate::GetChannelString())));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf("prodversion=%s", chrome::VersionInfo().Version().c_str())));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf("lang=%s", ChromeOmahaQueryParamsDelegate::GetLang())));
}

TEST(ChromeOmahaQueryParamsDelegateTest, GetParams) {
  TestParams(omaha_client::OmahaQueryParams::CRX);
  TestParams(omaha_client::OmahaQueryParams::CHROME);
}
