// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_finder.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/npapi/plugin_list.h"

using base::DictionaryValue;
using webkit::npapi::PluginGroup;
using webkit::npapi::PluginList;

TEST(PluginFinderTest, JsonSyntax) {
  scoped_ptr<DictionaryValue> plugin_list(PluginFinder::LoadPluginList());
  ASSERT_TRUE(plugin_list.get());
  for (DictionaryValue::Iterator plugin_it(*plugin_list);
       plugin_it.HasNext(); plugin_it.Advance()) {
    const DictionaryValue* plugin = NULL;
    ASSERT_TRUE(plugin_it.value().GetAsDictionary(&plugin));
    std::string dummy_str;
    bool dummy_bool;
    EXPECT_TRUE(plugin->GetString("lang", &dummy_str));
    EXPECT_TRUE(plugin->GetString("url", &dummy_str));
    EXPECT_TRUE(plugin->GetString("name", &dummy_str));
    if (plugin->HasKey("help_url"))
      EXPECT_TRUE(plugin->GetString("help_url", &dummy_str));
    if (plugin->HasKey("displayurl"))
      EXPECT_TRUE(plugin->GetBoolean("displayurl", &dummy_bool));
    if (plugin->HasKey("requires_authorization"))
      EXPECT_TRUE(plugin->GetBoolean("requires_authorization", &dummy_bool));
    ListValue* mime_types = NULL;
    ASSERT_TRUE(plugin->GetList("mime_types", &mime_types));
    for (ListValue::const_iterator mime_type_it = mime_types->begin();
         mime_type_it != mime_types->end(); ++mime_type_it) {
      EXPECT_TRUE((*mime_type_it)->GetAsString(&dummy_str));
    }
  }
}

TEST(PluginFinderTest, PluginGroups) {
  PluginFinder plugin_finder;
  PluginList* plugin_list = PluginList::Singleton();
  const std::vector<PluginGroup*>& plugin_groups =
      plugin_list->GetHardcodedPluginGroups();
  for (std::vector<PluginGroup*>::const_iterator it = plugin_groups.begin();
       it != plugin_groups.end(); ++it) {
    if ((*it)->version_ranges().empty())
      continue;
    std::string identifier = (*it)->identifier();
    EXPECT_TRUE(plugin_finder.FindPluginWithIdentifier(identifier)) <<
        "Couldn't find PluginInstaller for '" << identifier << "'";
  }
}
