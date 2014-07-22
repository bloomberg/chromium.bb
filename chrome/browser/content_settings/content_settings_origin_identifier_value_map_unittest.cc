// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_origin_identifier_value_map.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(OriginIdentifierValueMapTest, SetGetValue) {
  content_settings::OriginIdentifierValueMap map;

  EXPECT_EQ(NULL,
            map.GetValue(GURL("http://www.google.com"),
                         GURL("http://www.google.com"),
                         CONTENT_SETTINGS_TYPE_COOKIES,
                         std::string()));
  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               CONTENT_SETTINGS_TYPE_COOKIES,
               std::string(),
               new base::FundamentalValue(1));

  scoped_ptr<base::Value> expected_value(new base::FundamentalValue(1));
  EXPECT_TRUE(expected_value->Equals(map.GetValue(GURL("http://www.google.com"),
                                                  GURL("http://www.google.com"),
                                                  CONTENT_SETTINGS_TYPE_COOKIES,
                                                  std::string())));

  EXPECT_EQ(NULL,
            map.GetValue(GURL("http://www.google.com"),
                         GURL("http://www.youtube.com"),
                         CONTENT_SETTINGS_TYPE_COOKIES,
                         std::string()));

  EXPECT_EQ(NULL,
            map.GetValue(GURL("http://www.youtube.com"),
                         GURL("http://www.google.com"),
                         CONTENT_SETTINGS_TYPE_COOKIES,
                         std::string()));

  EXPECT_EQ(NULL,
            map.GetValue(GURL("http://www.google.com"),
                         GURL("http://www.google.com"),
                         CONTENT_SETTINGS_TYPE_POPUPS,
                         std::string()));

  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               CONTENT_SETTINGS_TYPE_COOKIES,
                               "resource_id"));
}

TEST(OriginIdentifierValueMapTest, SetDeleteValue) {
  content_settings::OriginIdentifierValueMap map;

  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               CONTENT_SETTINGS_TYPE_PLUGINS,
                               "java-plugin"));

  // Set sample values.
  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               CONTENT_SETTINGS_TYPE_PLUGINS,
               "java-plugin",
               new base::FundamentalValue(1));

  int actual_value;
  EXPECT_TRUE(map.GetValue(GURL("http://www.google.com"),
                           GURL("http://www.google.com"),
                           CONTENT_SETTINGS_TYPE_PLUGINS,
                           "java-plugin")->GetAsInteger(&actual_value));
  EXPECT_EQ(1, actual_value);
  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               CONTENT_SETTINGS_TYPE_PLUGINS,
                               "flash-plugin"));
  // Delete non-existing value.
  map.DeleteValue(
      ContentSettingsPattern::FromString("[*.]google.com"),
      ContentSettingsPattern::FromString("[*.]google.com"),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "flash-plugin");
  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               CONTENT_SETTINGS_TYPE_PLUGINS,
                               "flash-plugin"));
  EXPECT_TRUE(map.GetValue(GURL("http://www.google.com"),
                           GURL("http://www.google.com"),
                           CONTENT_SETTINGS_TYPE_PLUGINS,
                           "java-plugin")->GetAsInteger(&actual_value));
  EXPECT_EQ(1, actual_value);

  // Delete existing value.
  map.DeleteValue(
      ContentSettingsPattern::FromString("[*.]google.com"),
      ContentSettingsPattern::FromString("[*.]google.com"),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "java-plugin");

  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               CONTENT_SETTINGS_TYPE_PLUGINS,
                               "java-plugin"));
}

TEST(OriginIdentifierValueMapTest, Clear) {
  content_settings::OriginIdentifierValueMap map;
  EXPECT_TRUE(map.empty());

  // Set two values.
  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               CONTENT_SETTINGS_TYPE_PLUGINS,
               "java-plugin",
               new base::FundamentalValue(1));
  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               CONTENT_SETTINGS_TYPE_COOKIES,
               std::string(),
               new base::FundamentalValue(1));
  EXPECT_FALSE(map.empty());
  int actual_value;
  EXPECT_TRUE(map.GetValue(GURL("http://www.google.com"),
                           GURL("http://www.google.com"),
                           CONTENT_SETTINGS_TYPE_PLUGINS,
                           "java-plugin")->GetAsInteger(&actual_value));
  EXPECT_EQ(1, actual_value);

  // Clear the map.
  map.clear();
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               CONTENT_SETTINGS_TYPE_PLUGINS,
                               "java-plugin"));
}

TEST(OriginIdentifierValueMapTest, ListEntryPrecedences) {
  content_settings::OriginIdentifierValueMap map;

  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               CONTENT_SETTINGS_TYPE_COOKIES,
               std::string(),
               new base::FundamentalValue(1));

  map.SetValue(ContentSettingsPattern::FromString("www.google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               CONTENT_SETTINGS_TYPE_COOKIES,
               std::string(),
               new base::FundamentalValue(2));

  int actual_value;
  EXPECT_TRUE(map.GetValue(GURL("http://mail.google.com"),
                           GURL("http://www.google.com"),
                           CONTENT_SETTINGS_TYPE_COOKIES,
                           std::string())->GetAsInteger(&actual_value));
  EXPECT_EQ(1, actual_value);

  EXPECT_TRUE(map.GetValue(GURL("http://www.google.com"),
                           GURL("http://www.google.com"),
                           CONTENT_SETTINGS_TYPE_COOKIES,
                           std::string())->GetAsInteger(&actual_value));
  EXPECT_EQ(2, actual_value);
}

TEST(OriginIdentifierValueMapTest, IterateEmpty) {
  content_settings::OriginIdentifierValueMap map;
  scoped_ptr<content_settings::RuleIterator> rule_iterator(
      map.GetRuleIterator(CONTENT_SETTINGS_TYPE_COOKIES, std::string(), NULL));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST(OriginIdentifierValueMapTest, IterateNonempty) {
  // Verify the precedence order.
  content_settings::OriginIdentifierValueMap map;
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]google.com");
  ContentSettingsPattern sub_pattern =
      ContentSettingsPattern::FromString("sub.google.com");
  map.SetValue(pattern,
               ContentSettingsPattern::Wildcard(),
               CONTENT_SETTINGS_TYPE_COOKIES,
               std::string(),
               new base::FundamentalValue(1));
  map.SetValue(sub_pattern,
               ContentSettingsPattern::Wildcard(),
               CONTENT_SETTINGS_TYPE_COOKIES,
               std::string(),
               new base::FundamentalValue(2));

  scoped_ptr<content_settings::RuleIterator> rule_iterator(
      map.GetRuleIterator(CONTENT_SETTINGS_TYPE_COOKIES, std::string(), NULL));
  ASSERT_TRUE(rule_iterator->HasNext());
  content_settings::Rule rule = rule_iterator->Next();
  EXPECT_EQ(sub_pattern, rule.primary_pattern);
  EXPECT_EQ(2, content_settings::ValueToContentSetting(rule.value.get()));

  ASSERT_TRUE(rule_iterator->HasNext());
  rule = rule_iterator->Next();
  EXPECT_EQ(pattern, rule.primary_pattern);
  EXPECT_EQ(1, content_settings::ValueToContentSetting(rule.value.get()));
}
