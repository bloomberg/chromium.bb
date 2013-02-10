// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_installer.h"

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/webplugininfo.h"

using webkit::WebPluginInfo;

namespace {

PluginInstaller::SecurityStatus GetSecurityStatus(PluginInstaller* installer,
                                                  const char* version) {
  WebPluginInfo plugin(ASCIIToUTF16("Foo plug-in"),
                       base::FilePath(FILE_PATH_LITERAL("/tmp/plugin.so")),
                       ASCIIToUTF16(version),
                       ASCIIToUTF16("Foo plug-in."));
  return installer->GetSecurityStatus(plugin);
}

}  // namespace

TEST(PluginInstallerTest, SecurityStatus) {
  const PluginInstaller::SecurityStatus kUpToDate =
      PluginInstaller::SECURITY_STATUS_UP_TO_DATE;
  const PluginInstaller::SecurityStatus kOutOfDate =
      PluginInstaller::SECURITY_STATUS_OUT_OF_DATE;
  const PluginInstaller::SecurityStatus kRequiresAuthorization =
      PluginInstaller::SECURITY_STATUS_REQUIRES_AUTHORIZATION;

  PluginInstaller installer("claybrick-writer",
                            ASCIIToUTF16("ClayBrick Writer"),
                            true, GURL(), GURL(), ASCIIToUTF16("ClayBrick"));

#if defined(OS_LINUX)
  EXPECT_EQ(kRequiresAuthorization, GetSecurityStatus(&installer, "1.2.3"));
#else
  EXPECT_EQ(kUpToDate, GetSecurityStatus(&installer, "1.2.3"));
#endif

  installer.AddVersion(Version("9.4.1"), kRequiresAuthorization);
  installer.AddVersion(Version("10"), kOutOfDate);
  installer.AddVersion(Version("10.2.1"), kUpToDate);

  // Invalid version.
  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&installer, "foo"));

  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&installer, "0"));
  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&installer, "1.2.3"));
  EXPECT_EQ(kRequiresAuthorization, GetSecurityStatus(&installer, "9.4.1"));
  EXPECT_EQ(kRequiresAuthorization, GetSecurityStatus(&installer, "9.4.2"));
  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&installer, "10.2.0"));
  EXPECT_EQ(kUpToDate, GetSecurityStatus(&installer, "10.2.1"));
  EXPECT_EQ(kUpToDate, GetSecurityStatus(&installer, "11"));
}
