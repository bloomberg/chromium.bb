// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_metadata.h"

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/webplugininfo.h"

using webkit::WebPluginInfo;

namespace {

PluginMetadata::SecurityStatus GetSecurityStatus(
    PluginMetadata* plugin_metadata,
    const char* version) {
  WebPluginInfo plugin(ASCIIToUTF16("Foo plug-in"),
                       base::FilePath(FILE_PATH_LITERAL("/tmp/plugin.so")),
                       ASCIIToUTF16(version),
                       ASCIIToUTF16("Foo plug-in."));
  return plugin_metadata->GetSecurityStatus(plugin);
}

}  // namespace

TEST(PluginMetadataTest, SecurityStatus) {
  const PluginMetadata::SecurityStatus kUpToDate =
      PluginMetadata::SECURITY_STATUS_UP_TO_DATE;
  const PluginMetadata::SecurityStatus kOutOfDate =
      PluginMetadata::SECURITY_STATUS_OUT_OF_DATE;
  const PluginMetadata::SecurityStatus kRequiresAuthorization =
      PluginMetadata::SECURITY_STATUS_REQUIRES_AUTHORIZATION;

  PluginMetadata plugin_metadata("claybrick-writer",
                                 ASCIIToUTF16("ClayBrick Writer"),
                                 true, GURL(), GURL(),
                                 ASCIIToUTF16("ClayBrick"),
                                 "");
#if defined(OS_LINUX)
  EXPECT_EQ(kRequiresAuthorization,
            GetSecurityStatus(&plugin_metadata, "1.2.3"));
#else
  EXPECT_EQ(kUpToDate, GetSecurityStatus(&plugin_metadata, "1.2.3"));
#endif

  plugin_metadata.AddVersion(Version("9.4.1"), kRequiresAuthorization);
  plugin_metadata.AddVersion(Version("10"), kOutOfDate);
  plugin_metadata.AddVersion(Version("10.2.1"), kUpToDate);

  // Invalid version.
  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&plugin_metadata, "foo"));

  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&plugin_metadata, "0"));
  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&plugin_metadata, "1.2.3"));
  EXPECT_EQ(kRequiresAuthorization,
            GetSecurityStatus(&plugin_metadata, "9.4.1"));
  EXPECT_EQ(kRequiresAuthorization,
            GetSecurityStatus(&plugin_metadata, "9.4.2"));
  EXPECT_EQ(kOutOfDate, GetSecurityStatus(&plugin_metadata, "10.2.0"));
  EXPECT_EQ(kUpToDate, GetSecurityStatus(&plugin_metadata, "10.2.1"));
  EXPECT_EQ(kUpToDate, GetSecurityStatus(&plugin_metadata, "11"));
}
