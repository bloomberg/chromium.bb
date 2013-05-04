// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/stringprintf.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/common/omaha_query_params/omaha_query_params.h"
#include "extensions/common/id_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPrintf;
using chrome::OmahaQueryParams;

namespace extensions {

// Returns true if |target| is found in |source|.
bool Contains(const std::string& source, const std::string& target) {
  return source.find(target) != std::string::npos;
}

TEST(WebstoreInstallerTest, PlatformParams) {
  std::string id = extensions::id_util::GenerateId("some random string");
  GURL url = WebstoreInstaller::GetWebstoreInstallURL(id, "");
  std::string query = url.query();
  EXPECT_TRUE(Contains(query,StringPrintf("os=%s", OmahaQueryParams::getOS())));
  EXPECT_TRUE(Contains(query,StringPrintf("arch=%s",
                                          OmahaQueryParams::getArch())));
  EXPECT_TRUE(Contains(query,StringPrintf("nacl_arch=%s",
                                          OmahaQueryParams::getNaclArch())));

}

}  // namespace extensions
