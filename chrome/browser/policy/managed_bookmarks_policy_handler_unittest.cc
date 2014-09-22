// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "chrome/browser/policy/managed_bookmarks_policy_handler.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "policy/policy_constants.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/common/value_builder.h"
#endif

namespace policy {

class ManagedBookmarksPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
  virtual void SetUp() OVERRIDE {
    Schema chrome_schema = Schema::Wrap(GetChromeSchemaData());
    handler_list_.AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
        new ManagedBookmarksPolicyHandler(chrome_schema)));
  }
};

#if defined(ENABLE_EXTENSIONS)
TEST_F(ManagedBookmarksPolicyHandlerTest, ApplyPolicySettings) {
  EXPECT_FALSE(store_->GetValue(bookmarks::prefs::kManagedBookmarks, NULL));

  PolicyMap policy;
  policy.Set(key::kManagedBookmarks,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             base::JSONReader::Read(
                 "["
                 "  {"
                 "    \"name\": \"Google\","
                 "    \"url\": \"google.com\""
                 "  },"
                 "  {"
                 "    \"name\": \"Empty Folder\","
                 "    \"children\": []"
                 "  },"
                 "  {"
                 "    \"name\": \"Big Folder\","
                 "    \"children\": ["
                 "      {"
                 "        \"name\": \"Youtube\","
                 "        \"url\": \"youtube.com\""
                 "      },"
                 "      {"
                 "        \"name\": \"Chromium\","
                 "        \"url\": \"chromium.org\""
                 "      },"
                 "      {"
                 "        \"name\": \"More Stuff\","
                 "        \"children\": ["
                 "          {"
                 "            \"name\": \"Bugs\","
                 "            \"url\": \"crbug.com\""
                 "          }"
                 "        ]"
                 "      }"
                 "    ]"
                 "  }"
                 "]"),
             NULL);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = NULL;
  EXPECT_TRUE(
      store_->GetValue(bookmarks::prefs::kManagedBookmarks, &pref_value));
  ASSERT_TRUE(pref_value);

  scoped_ptr<base::Value> expected(
      extensions::ListBuilder()
          .Append(extensions::DictionaryBuilder()
              .Set("name", "Google")
              .Set("url", "http://google.com/"))
          .Append(extensions::DictionaryBuilder()
              .Set("name", "Empty Folder")
              .Set("children", extensions::ListBuilder().Pass()))
          .Append(extensions::DictionaryBuilder()
              .Set("name", "Big Folder")
              .Set("children", extensions::ListBuilder()
                  .Append(extensions::DictionaryBuilder()
                      .Set("name", "Youtube")
                      .Set("url", "http://youtube.com/"))
                  .Append(extensions::DictionaryBuilder()
                      .Set("name", "Chromium")
                      .Set("url", "http://chromium.org/"))
                  .Append(extensions::DictionaryBuilder()
                      .Set("name", "More Stuff")
                      .Set("children", extensions::ListBuilder()
                          .Append(extensions::DictionaryBuilder()
                              .Set("name", "Bugs")
                              .Set("url", "http://crbug.com/")
                              .Pass())
                          .Pass())
                      .Pass())
                  .Pass())
              .Pass())
          .Build());
  EXPECT_TRUE(pref_value->Equals(expected.get()));
}
#endif  // defined(ENABLE_EXTENSIONS)

TEST_F(ManagedBookmarksPolicyHandlerTest, WrongPolicyType) {
  PolicyMap policy;
  // The expected type is base::ListValue, but this policy sets it as an
  // unparsed base::StringValue. Any type other than ListValue should fail.
  policy.Set(key::kManagedBookmarks,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(
                 "["
                 "  {"
                 "    \"name\": \"Google\","
                 "    \"url\": \"google.com\""
                 "  },"
                 "]"),
             NULL);
  UpdateProviderPolicy(policy);
  EXPECT_FALSE(store_->GetValue(bookmarks::prefs::kManagedBookmarks, NULL));
}

#if defined(ENABLE_EXTENSIONS)
TEST_F(ManagedBookmarksPolicyHandlerTest, UnknownKeys) {
  PolicyMap policy;
  policy.Set(key::kManagedBookmarks,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             base::JSONReader::Read(
                 "["
                 "  {"
                 "    \"name\": \"Google\","
                 "    \"unknown\": \"should be ignored\","
                 "    \"url\": \"google.com\""
                 "  }"
                 "]"),
             NULL);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = NULL;
  EXPECT_TRUE(
      store_->GetValue(bookmarks::prefs::kManagedBookmarks, &pref_value));
  ASSERT_TRUE(pref_value);

  scoped_ptr<base::Value> expected(
      extensions::ListBuilder()
          .Append(extensions::DictionaryBuilder()
              .Set("name", "Google")
              .Set("url", "http://google.com/"))
          .Build());
  EXPECT_TRUE(pref_value->Equals(expected.get()));
}
#endif

#if defined(ENABLE_EXTENSIONS)
TEST_F(ManagedBookmarksPolicyHandlerTest, BadBookmark) {
  PolicyMap policy;
  policy.Set(key::kManagedBookmarks,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             base::JSONReader::Read(
                 "["
                 "  {"
                 "    \"name\": \"Empty\","
                 "    \"url\": \"\""
                 "  },"
                 "  {"
                 "    \"name\": \"Invalid type\","
                 "    \"url\": 4"
                 "  },"
                 "  {"
                 "    \"name\": \"Invalid URL\","
                 "    \"url\": \"?\""
                 "  },"
                 "  {"
                 "    \"name\": \"Google\","
                 "    \"url\": \"google.com\""
                 "  }"
                 "]"),
             NULL);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = NULL;
  EXPECT_TRUE(
      store_->GetValue(bookmarks::prefs::kManagedBookmarks, &pref_value));
  ASSERT_TRUE(pref_value);

  scoped_ptr<base::Value> expected(
      extensions::ListBuilder()
          .Append(extensions::DictionaryBuilder()
              .Set("name", "Google")
              .Set("url", "http://google.com/"))
          .Build());
  EXPECT_TRUE(pref_value->Equals(expected.get()));
}
#endif

}  // namespace policy
