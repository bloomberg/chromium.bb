// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_finder.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::ListValue;

TEST(PluginFinderTest, JsonSyntax) {
  scoped_ptr<ListValue> plugin_list(PluginFinder::LoadPluginList());
  ASSERT_TRUE(plugin_list.get());
  for (ListValue::const_iterator plugin_it = plugin_list->begin();
       plugin_it != plugin_list->end(); ++plugin_it) {
    const DictionaryValue* plugin = NULL;
    ASSERT_TRUE((*plugin_it)->GetAsDictionary(&plugin));
    std::string dummy_str;
    EXPECT_TRUE(plugin->GetString("lang", &dummy_str));
    EXPECT_TRUE(plugin->GetString("identifier", &dummy_str));
    EXPECT_TRUE(plugin->GetString("url", &dummy_str));
    EXPECT_TRUE(plugin->GetString("name", &dummy_str));
    ListValue* mime_types = NULL;
    ASSERT_TRUE(plugin->GetList("mime_types", &mime_types));
    for (ListValue::const_iterator mime_type_it = mime_types->begin();
         mime_type_it != mime_types->end(); ++mime_type_it) {
      EXPECT_TRUE((*mime_type_it)->GetAsString(&dummy_str));
    }
  }
}
