// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_finder.h"

#include "base/values.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::ListValue;

TEST(PluginFinderTest, JsonSyntax) {
  std::unique_ptr<base::DictionaryValue> plugin_list(
      PluginFinder::LoadBuiltInPluginList());
  ASSERT_TRUE(plugin_list.get());
  std::unique_ptr<base::Value> version;
  ASSERT_TRUE(plugin_list->Remove("x-version", &version));
  EXPECT_EQ(base::Value::Type::INTEGER, version->type());

  for (base::DictionaryValue::Iterator plugin_it(*plugin_list);
       !plugin_it.IsAtEnd(); plugin_it.Advance()) {
    const base::DictionaryValue* plugin = NULL;
    ASSERT_TRUE(plugin_it.value().GetAsDictionary(&plugin));
    std::string dummy_str;
    bool dummy_bool;
    if (plugin->HasKey("lang"))
      EXPECT_TRUE(plugin->GetString("lang", &dummy_str));
    if (plugin->HasKey("url"))
      EXPECT_TRUE(plugin->GetString("url", &dummy_str));
    EXPECT_TRUE(plugin->GetString("name", &dummy_str));
    if (plugin->HasKey("help_url"))
      EXPECT_TRUE(plugin->GetString("help_url", &dummy_str));
    bool display_url = false;
    if (plugin->HasKey("displayurl")) {
      EXPECT_TRUE(plugin->GetBoolean("displayurl", &display_url));
      EXPECT_TRUE(display_url);
    }
    if (plugin->HasKey("requires_authorization"))
      EXPECT_TRUE(plugin->GetBoolean("requires_authorization", &dummy_bool));
    const base::ListValue* mime_types = NULL;
    if (plugin->GetList("mime_types", &mime_types)) {
      for (base::ListValue::const_iterator mime_type_it = mime_types->begin();
           mime_type_it != mime_types->end(); ++mime_type_it) {
        EXPECT_TRUE(mime_type_it->GetAsString(&dummy_str));
      }
    }

    const base::ListValue* matching_mime_types = NULL;
    if (plugin->GetList("matching_mime_types", &matching_mime_types)) {
      for (base::ListValue::const_iterator it = matching_mime_types->begin();
           it != matching_mime_types->end(); ++it) {
        EXPECT_TRUE(it->GetAsString(&dummy_str));
      }
    }

    const base::ListValue* versions = NULL;
    if (!plugin->GetList("versions", &versions))
      continue;

    for (base::ListValue::const_iterator it = versions->begin();
         it != versions->end(); ++it) {
      const base::DictionaryValue* version_dict = NULL;
      ASSERT_TRUE(it->GetAsDictionary(&version_dict));
      EXPECT_TRUE(version_dict->GetString("version", &dummy_str));
      std::string status_str;
      EXPECT_TRUE(version_dict->GetString("status", &status_str));
      PluginMetadata::SecurityStatus status =
          PluginMetadata::SECURITY_STATUS_UP_TO_DATE;
      EXPECT_TRUE(PluginMetadata::ParseSecurityStatus(status_str, &status))
          << "Invalid security status \"" << status_str << "\"";
    }
  }
}
