// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/plugin_group.h"

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/plugins/webplugininfo.h"

static const PluginGroupDefinition kPluginDef = {
    "MyPlugin", "MyPlugin", "", "", "3.0.44", "http://latest/" };
static const PluginGroupDefinition kPluginDef3 = {
    "MyPlugin 3", "MyPlugin", "0", "4", "3.0.44", "http://latest" };
static const PluginGroupDefinition kPluginDef4 = {
    "MyPlugin 4", "MyPlugin", "4", "5", "4.0.44", "http://latest" };
static const PluginGroupDefinition kPluginDefNotVulnerable = {
    "MyPlugin", "MyPlugin", "", "", "", "http://latest" };

// name, path, version, desc, mime_types, enabled.
static WebPluginInfo kPlugin2043 = {
    ASCIIToUTF16("MyPlugin"), FilePath(), ASCIIToUTF16("2.0.43"), string16(),
    std::vector<WebPluginMimeType>(), true };
static WebPluginInfo kPlugin3043 = {
    ASCIIToUTF16("MyPlugin"), FilePath(), ASCIIToUTF16("3.0.43"), string16(),
    std::vector<WebPluginMimeType>(), true };
static WebPluginInfo kPlugin3044 = {
    ASCIIToUTF16("MyPlugin"), FilePath(), ASCIIToUTF16("3.0.44"), string16(),
    std::vector<WebPluginMimeType>(), true };
static WebPluginInfo kPlugin3045 = {
    ASCIIToUTF16("MyPlugin"), FilePath(), ASCIIToUTF16("3.0.45"), string16(),
    std::vector<WebPluginMimeType>(), true };
static WebPluginInfo kPlugin4043 = {
    ASCIIToUTF16("MyPlugin"), FilePath(), ASCIIToUTF16("4.0.43"), string16(),
    std::vector<WebPluginMimeType>(), true };

class PluginGroupTest : public testing::Test {
};

TEST(PluginGroupTest, PluginGroupMatch) {
  scoped_ptr<PluginGroup> group(PluginGroup::FromPluginGroupDefinition(
      kPluginDef3));
  EXPECT_TRUE(group->Match(kPlugin3045));
  group->AddPlugin(kPlugin3045, 0);
  EXPECT_FALSE(group->IsVulnerable());
}

TEST(PluginGroupTest, PluginGroupMatchMultipleFiles) {
  scoped_ptr<PluginGroup> group(PluginGroup::FromPluginGroupDefinition(
      kPluginDef3));
  EXPECT_TRUE(group->Match(kPlugin3043));
  group->AddPlugin(kPlugin3043, 0);
  EXPECT_TRUE(group->IsVulnerable());

  EXPECT_TRUE(group->Match(kPlugin3045));
  group->AddPlugin(kPlugin3045, 1);
  EXPECT_FALSE(group->IsVulnerable());
}

TEST(PluginGroupTest, PluginGroupMatchCorrectVersion) {
  scoped_ptr<PluginGroup> group(PluginGroup::FromPluginGroupDefinition(
      kPluginDef3));
  EXPECT_TRUE(group->Match(kPlugin2043));
  EXPECT_TRUE(group->Match(kPlugin3043));
  EXPECT_FALSE(group->Match(kPlugin4043));

  group.reset(PluginGroup::FromPluginGroupDefinition(kPluginDef4));
  EXPECT_FALSE(group->Match(kPlugin2043));
  EXPECT_FALSE(group->Match(kPlugin3043));
  EXPECT_TRUE(group->Match(kPlugin4043));
}

TEST(PluginGroupTest, PluginGroupDefinition) {
  const PluginGroupDefinition* definitions =
      PluginGroup::GetPluginGroupDefinitions();
  for (size_t i = 0; i < PluginGroup::GetPluginGroupDefinitionsSize(); ++i) {
    scoped_ptr<PluginGroup> def_group(
        PluginGroup::FromPluginGroupDefinition(definitions[i]));
    ASSERT_TRUE(def_group.get() != NULL);
    EXPECT_FALSE(def_group->Match(kPlugin2043));
  }
}

TEST(PluginGroupTest, VersionExtraction) {
  // Some real-world plugin versions (spaces, commata, parentheses, 'r', oh my)
  const char* versions[][2] = {
    { "7.6.6 (1671)", "7.6.6.1671" },  // Quicktime
    { "2, 0, 0, 254", "2.0.0.254" },   // DivX
    { "3, 0, 0, 0", "3.0.0.0" },       // Picasa
    { "1, 0, 0, 1", "1.0.0.1" },       // Earth
    { "10,0,45,2", "10.0.45.2" },      // Flash
    { "11.5.7r609", "11.5.7.609"}     // Shockwave
  };

  for (size_t i = 0; i < arraysize(versions); i++) {
    const WebPluginInfo plugin = {
        ASCIIToUTF16("Blah Plugin"), FilePath(), ASCIIToUTF16(versions[i][0]),
        string16(),std::vector<WebPluginMimeType>(), true };
    scoped_ptr<PluginGroup> group(PluginGroup::FromWebPluginInfo(plugin));
    EXPECT_TRUE(group->Match(plugin));
    group->AddPlugin(plugin, 0);
    scoped_ptr<DictionaryValue> data(group->GetDataForUI());
    std::string version;
    data->GetString(L"version", &version);
    EXPECT_EQ(versions[i][1], version);
  }
}
