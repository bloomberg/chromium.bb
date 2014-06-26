// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "components/omaha_query_params/omaha_query_params.h"
#include "components/omaha_query_params/omaha_query_params_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPrintf;

namespace omaha_query_params {

namespace {

bool Contains(const std::string& source, const std::string& target) {
  return source.find(target) != std::string::npos;
}

class TestOmahaQueryParamsDelegate : public OmahaQueryParamsDelegate {
  virtual std::string GetExtraParams() OVERRIDE { return "&cat=dog"; }
};

}  // namespace

void TestParams(OmahaQueryParams::ProdId prod_id, bool extra_params) {
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
  if (extra_params)
    EXPECT_TRUE(Contains(params, "cat=dog"));
}

TEST(OmahaQueryParamsTest, GetParams) {
  TestParams(OmahaQueryParams::CRX, false);
  TestParams(OmahaQueryParams::CHROME, false);

  TestOmahaQueryParamsDelegate delegate;
  OmahaQueryParams::SetDelegate(&delegate);

  TestParams(OmahaQueryParams::CRX, true);
  TestParams(OmahaQueryParams::CHROME, true);
}

}  // namespace omaha_query_params
