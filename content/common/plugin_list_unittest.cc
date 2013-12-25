// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/plugin_list.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

base::FilePath::CharType kFooPath[] = FILE_PATH_LITERAL("/plugins/foo.plugin");
base::FilePath::CharType kBarPath[] = FILE_PATH_LITERAL("/plugins/bar.plugin");
const char* kFooName = "Foo Plugin";
const char* kFooMimeType = "application/x-foo-mime-type";
const char* kFooFileType = "foo";

bool Equals(const WebPluginInfo& a, const WebPluginInfo& b) {
  return (a.name == b.name &&
          a.path == b.path &&
          a.version == b.version &&
          a.desc == b.desc);
}

bool Contains(const std::vector<WebPluginInfo>& list,
              const WebPluginInfo& plugin) {
  for (std::vector<WebPluginInfo>::const_iterator it = list.begin();
       it != list.end(); ++it) {
    if (Equals(*it, plugin))
      return true;
  }
  return false;
}

}  // namespace

class PluginListTest : public testing::Test {
 public:
  PluginListTest()
      : foo_plugin_(base::ASCIIToUTF16(kFooName),
                    base::FilePath(kFooPath),
                    base::ASCIIToUTF16("1.2.3"),
                    base::ASCIIToUTF16("foo")),
        bar_plugin_(base::ASCIIToUTF16("Bar Plugin"),
                    base::FilePath(kBarPath),
                    base::ASCIIToUTF16("2.3.4"),
                    base::ASCIIToUTF16("bar")) {
  }

  virtual void SetUp() {
    plugin_list_.DisablePluginsDiscovery();
    plugin_list_.RegisterInternalPlugin(bar_plugin_, false);
    foo_plugin_.mime_types.push_back(
        WebPluginMimeType(kFooMimeType, kFooFileType, std::string()));
    plugin_list_.RegisterInternalPlugin(foo_plugin_, false);
  }

 protected:
  PluginList plugin_list_;
  WebPluginInfo foo_plugin_;
  WebPluginInfo bar_plugin_;
};

TEST_F(PluginListTest, GetPlugins) {
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetPlugins(&plugins, true);
  EXPECT_EQ(2u, plugins.size());
  EXPECT_TRUE(Contains(plugins, foo_plugin_));
  EXPECT_TRUE(Contains(plugins, bar_plugin_));
}

TEST_F(PluginListTest, BadPluginDescription) {
  WebPluginInfo plugin_3043(
      base::string16(), base::FilePath(FILE_PATH_LITERAL("/myplugin.3.0.43")),
      base::string16(), base::string16());
  // Simulate loading of the plugins.
  plugin_list_.RegisterInternalPlugin(plugin_3043, false);
  // Now we should have them in the state we specified above.
  plugin_list_.RefreshPlugins();
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetPlugins(&plugins, true);
  ASSERT_TRUE(Contains(plugins, plugin_3043));
}

TEST_F(PluginListTest, GetPluginInfoArray) {
  const char kTargetUrl[] = "http://example.com/test.foo";
  GURL target_url(kTargetUrl);
  std::vector<WebPluginInfo> plugins;
  std::vector<std::string> actual_mime_types;

  // The file type of the URL is supported by foo_plugin_. However,
  // GetPluginInfoArray should not match foo_plugin_ because the MIME type is
  // application/octet-stream.
  plugin_list_.GetPluginInfoArray(target_url,
                                  "application/octet-stream",
                                  false, // allow_wildcard
                                  NULL,  // use_stale
                                  false, // include_npapi
                                  &plugins,
                                  &actual_mime_types);
  EXPECT_EQ(0u, plugins.size());
  EXPECT_EQ(0u, actual_mime_types.size());

  // foo_plugin_ matches due to the MIME type.
  plugins.clear();
  actual_mime_types.clear();
  plugin_list_.GetPluginInfoArray(target_url,
                                  kFooMimeType,
                                  false, // allow_wildcard
                                  NULL,  // use_stale
                                  false, // include_npapi
                                  &plugins,
                                  &actual_mime_types);
  EXPECT_EQ(1u, plugins.size());
  EXPECT_TRUE(Contains(plugins, foo_plugin_));
  ASSERT_EQ(1u, actual_mime_types.size());
  EXPECT_EQ(kFooMimeType, actual_mime_types.front());

  // foo_plugin_ matches due to the file type and empty MIME type.
  plugins.clear();
  actual_mime_types.clear();
  plugin_list_.GetPluginInfoArray(target_url,
                                  "",
                                  false, // allow_wildcard
                                  NULL,  // use_stale
                                  false, // include_npapi
                                  &plugins,
                                  &actual_mime_types);
  EXPECT_EQ(1u, plugins.size());
  EXPECT_TRUE(Contains(plugins, foo_plugin_));
  ASSERT_EQ(1u, actual_mime_types.size());
  EXPECT_EQ(kFooMimeType, actual_mime_types.front());
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)

// Test parsing a simple description: Real Audio.
TEST(MIMEDescriptionParse, Simple) {
  std::vector<WebPluginMimeType> types;
  PluginList::ParseMIMEDescription(
      "audio/x-pn-realaudio-plugin:rpm:RealAudio document;",
      &types);
  ASSERT_EQ(1U, types.size());
  const WebPluginMimeType& type = types[0];
  EXPECT_EQ("audio/x-pn-realaudio-plugin", type.mime_type);
  ASSERT_EQ(1U, type.file_extensions.size());
  EXPECT_EQ("rpm", type.file_extensions[0]);
  EXPECT_EQ(base::ASCIIToUTF16("RealAudio document"), type.description);
}

// Test parsing a multi-entry description: QuickTime as provided by Totem.
TEST(MIMEDescriptionParse, Multi) {
  std::vector<WebPluginMimeType> types;
  PluginList::ParseMIMEDescription(
      "video/quicktime:mov:QuickTime video;video/mp4:mp4:MPEG-4 "
      "video;image/x-macpaint:pntg:MacPaint Bitmap image;image/x"
      "-quicktime:pict, pict1, pict2:QuickTime image;video/x-m4v"
      ":m4v:MPEG-4 video;",
      &types);

  ASSERT_EQ(5U, types.size());

  // Check the x-quicktime one, since it looks tricky with spaces in the
  // extension list.
  const WebPluginMimeType& type = types[3];
  EXPECT_EQ("image/x-quicktime", type.mime_type);
  ASSERT_EQ(3U, type.file_extensions.size());
  EXPECT_EQ("pict2", type.file_extensions[2]);
  EXPECT_EQ(base::ASCIIToUTF16("QuickTime image"), type.description);
}

// Test parsing a Japanese description, since we got this wrong in the past.
// This comes from loading Totem with LANG=ja_JP.UTF-8.
TEST(MIMEDescriptionParse, JapaneseUTF8) {
  std::vector<WebPluginMimeType> types;
  PluginList::ParseMIMEDescription(
      "audio/x-ogg:ogg:Ogg \xe3\x82\xaa\xe3\x83\xbc\xe3\x83\x87"
      "\xe3\x82\xa3\xe3\x83\xaa",
      &types);

  ASSERT_EQ(1U, types.size());
  // Check we got the right number of Unicode characters out of the parse.
  EXPECT_EQ(9U, types[0].description.size());
}

// Test that we handle corner cases gracefully.
TEST(MIMEDescriptionParse, CornerCases) {
  std::vector<WebPluginMimeType> types;
  PluginList::ParseMIMEDescription("mime/type:", &types);
  EXPECT_TRUE(types.empty());

  types.clear();
  PluginList::ParseMIMEDescription("mime/type:ext1:", &types);
  ASSERT_EQ(1U, types.size());
  EXPECT_EQ("mime/type", types[0].mime_type);
  EXPECT_EQ(1U, types[0].file_extensions.size());
  EXPECT_EQ("ext1", types[0].file_extensions[0]);
  EXPECT_EQ(base::string16(), types[0].description);
}

// This Java plugin has embedded semicolons in the mime type.
TEST(MIMEDescriptionParse, ComplicatedJava) {
  std::vector<WebPluginMimeType> types;
  PluginList::ParseMIMEDescription(
      "application/x-java-vm:class,jar:IcedTea;application/x-java"
      "-applet:class,jar:IcedTea;application/x-java-applet;versio"
      "n=1.1:class,jar:IcedTea;application/x-java-applet;version="
      "1.1.1:class,jar:IcedTea;application/x-java-applet;version="
      "1.1.2:class,jar:IcedTea;application/x-java-applet;version="
      "1.1.3:class,jar:IcedTea;application/x-java-applet;version="
      "1.2:class,jar:IcedTea;application/x-java-applet;version=1."
      "2.1:class,jar:IcedTea;application/x-java-applet;version=1."
      "2.2:class,jar:IcedTea;application/x-java-applet;version=1."
      "3:class,jar:IcedTea;application/x-java-applet;version=1.3."
      "1:class,jar:IcedTea;application/x-java-applet;version=1.4:"
      "class,jar:IcedTea",
      &types);

  ASSERT_EQ(12U, types.size());
  for (size_t i = 0; i < types.size(); ++i)
    EXPECT_EQ(base::ASCIIToUTF16("IcedTea"), types[i].description);

  // Verify that the mime types with semis are coming through ok.
  EXPECT_TRUE(types[4].mime_type.find(';') != std::string::npos);
}

// Make sure we understand how to get the version numbers for common Linux
// plug-ins.
TEST(PluginDescriptionParse, ExtractVersion) {
  WebPluginInfo info;
  PluginList::ExtractVersionString("Shockwave Flash 10.1 r102", &info);
  EXPECT_EQ(base::ASCIIToUTF16("10.1 r102"), info.version);
  PluginList::ExtractVersionString("Java(TM) Plug-in 1.6.0_22", &info);
  EXPECT_EQ(base::ASCIIToUTF16("1.6.0_22"), info.version);
  // It's actually much more likely for a modern Linux distribution to have
  // IcedTea.
  PluginList::ExtractVersionString(
      "IcedTea-Web Plugin "
      "(using IcedTea-Web 1.2 (1.2-2ubuntu0.10.04.2))",
      &info);
  EXPECT_EQ(base::ASCIIToUTF16("1.2"), info.version);
}

#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)


}  // namespace content
