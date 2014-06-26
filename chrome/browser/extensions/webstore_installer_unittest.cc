// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/omaha_query_params/chrome_omaha_query_params_delegate.h"
#include "components/omaha_query_params/omaha_query_params.h"
#include "extensions/common/id_util.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPrintf;
using omaha_query_params::OmahaQueryParams;

namespace extensions {

// Returns true if |target| is found in |source|.
bool Contains(const std::string& source, const std::string& target) {
  return source.find(target) != std::string::npos;
}

TEST(WebstoreInstallerTest, PlatformParams) {
  std::string id = extensions::id_util::GenerateId("some random string");
  std::string source = "inline";
  GURL url = WebstoreInstaller::GetWebstoreInstallURL(id,
      WebstoreInstaller::INSTALL_SOURCE_INLINE);
  std::string query = url.query();
  EXPECT_TRUE(
      Contains(query, StringPrintf("os=%s", OmahaQueryParams::GetOS())));
  EXPECT_TRUE(
      Contains(query, StringPrintf("arch=%s", OmahaQueryParams::GetArch())));
  EXPECT_TRUE(Contains(
      query, StringPrintf("nacl_arch=%s", OmahaQueryParams::GetNaclArch())));
  EXPECT_TRUE(
      Contains(query,
               net::EscapeQueryParamValue(
                   StringPrintf("installsource=%s", source.c_str()), true)));
  EXPECT_TRUE(Contains(
      query,
      StringPrintf("lang=%s", ChromeOmahaQueryParamsDelegate::GetLang())));
}

}  // namespace extensions
