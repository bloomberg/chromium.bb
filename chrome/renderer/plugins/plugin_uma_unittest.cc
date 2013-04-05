// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "chrome/renderer/plugins/plugin_uma.h"

class PluginUMATest : public testing::Test {
 public:
  static void ExpectPluginType(
      PluginUMAReporter::PluginType expected_plugin_type,
      const std::string& plugin_mime_type,
      const GURL& plugin_src) {
    EXPECT_EQ(expected_plugin_type,
              PluginUMAReporter::GetInstance()->GetPluginType(plugin_mime_type,
                                                              plugin_src));
  }
};

TEST_F(PluginUMATest, WindowsMediaPlayer) {
  ExpectPluginType(PluginUMAReporter::WINDOWS_MEDIA_PLAYER,
                   "application/x-mplayer2",
                   GURL("file://some_file.mov"));
  ExpectPluginType(PluginUMAReporter::OTHER,
                   "application/x-mplayer2-some_sufix",
                   GURL("file://some_file.mov"));
  ExpectPluginType(PluginUMAReporter::OTHER,
                   "some-prefix-application/x-mplayer2",
                   GURL("file://some_file.mov"));
}

TEST_F(PluginUMATest, Silverlight) {
  ExpectPluginType(PluginUMAReporter::SILVERLIGHT,
                   "application/x-silverlight",
                   GURL("aaaa"));
  ExpectPluginType(PluginUMAReporter::SILVERLIGHT,
                   "application/x-silverlight-some-sufix",
                   GURL("aaaa"));
  ExpectPluginType(PluginUMAReporter::OTHER,
                   "some-prefix-application/x-silverlight",
                   GURL("aaaa"));
}

TEST_F(PluginUMATest, RealPlayer) {
  ExpectPluginType(PluginUMAReporter::REALPLAYER,
                   "audio/x-pn-realaudio",
                   GURL("some url"));
  ExpectPluginType(PluginUMAReporter::REALPLAYER,
                   "audio/x-pn-realaudio-some-sufix",
                   GURL("some url"));
  ExpectPluginType(PluginUMAReporter::OTHER,
                   "some-prefix-audio/x-pn-realaudio",
                   GURL("some url"));
}

TEST_F(PluginUMATest, Java) {
  ExpectPluginType(PluginUMAReporter::JAVA,
                   "application/x-java-applet",
                   GURL("some url"));
  ExpectPluginType(PluginUMAReporter::JAVA,
                   "application/x-java-applet-some-sufix",
                   GURL("some url"));
  ExpectPluginType(PluginUMAReporter::JAVA,
                   "some-prefix-application/x-java-applet-sufix",
                   GURL("some url"));
}

TEST_F(PluginUMATest, QuickTime) {
  ExpectPluginType(PluginUMAReporter::QUICKTIME,
                   "video/quicktime",
                   GURL("some url"));
  ExpectPluginType(PluginUMAReporter::OTHER,
                   "video/quicktime-sufix",
                   GURL("some url"));
  ExpectPluginType(PluginUMAReporter::OTHER,
                   "prefix-video/quicktime",
                   GURL("some url"));
}

TEST_F(PluginUMATest, BySrcExtension) {
  ExpectPluginType(PluginUMAReporter::QUICKTIME,
                   "",
                   GURL("file://file.mov"));

  // When plugin's mime type is given, we don't check extension.
  ExpectPluginType(PluginUMAReporter::OTHER,
                   "unknown-plugin",
                   GURL("http://file.mov"));

  ExpectPluginType(PluginUMAReporter::OTHER,
                   "",
                   GURL("http://file.unknown_extension"));
  ExpectPluginType(PluginUMAReporter::QUICKTIME,
                   "",
                   GURL("http://aaa/file.mov?x=aaaa&y=b#c"));
  ExpectPluginType(PluginUMAReporter::QUICKTIME,
                   "",
                   GURL("http://file.mov?x=aaaa&y=b#c"));
  ExpectPluginType(PluginUMAReporter::OTHER,
                   "",
                   GURL("http://"));
  ExpectPluginType(PluginUMAReporter::OTHER,
                   "",
                   GURL("mov"));
}

TEST_F(PluginUMATest, CaseSensitivity) {
  ExpectPluginType(PluginUMAReporter::QUICKTIME,
                   "video/QUICKTIME",
                   GURL("http://file.aaa"));
  ExpectPluginType(PluginUMAReporter::QUICKTIME,
                   "",
                   GURL("http://file.MoV"));
}
