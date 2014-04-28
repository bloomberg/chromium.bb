// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/default_search_manager.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Checks that the two TemplateURLs are similar. It does not check the id, the
// date_created or the last_modified time.  Neither pointer should be NULL.
void ExpectSimilar(const TemplateURLData* expected,
                   const TemplateURLData* actual) {
  ASSERT_TRUE(expected != NULL);
  ASSERT_TRUE(actual != NULL);

  EXPECT_EQ(expected->short_name, actual->short_name);
  EXPECT_EQ(expected->keyword(), actual->keyword());
  EXPECT_EQ(expected->url(), actual->url());
  EXPECT_EQ(expected->suggestions_url, actual->suggestions_url);
  EXPECT_EQ(expected->favicon_url, actual->favicon_url);
  EXPECT_EQ(expected->alternate_urls, actual->alternate_urls);
  EXPECT_EQ(expected->show_in_default_list, actual->show_in_default_list);
  EXPECT_EQ(expected->safe_for_autoreplace, actual->safe_for_autoreplace);
  EXPECT_EQ(expected->input_encodings, actual->input_encodings);
  EXPECT_EQ(expected->search_terms_replacement_key,
            actual->search_terms_replacement_key);
}

}  // namespace

class DefaultSearchManagerTest : public testing::Test {
 public:
  DefaultSearchManagerTest() {};

  virtual void SetUp() OVERRIDE {
    pref_service_.reset(new TestingPrefServiceSyncable);
    DefaultSearchManager::RegisterProfilePrefs(pref_service_->registry());
  }

  PrefService* pref_service() { return pref_service_.get(); }

 private:
  scoped_ptr<TestingPrefServiceSyncable> pref_service_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchManagerTest);
};

// Test that a TemplateURLData object is properly written and read from Prefs.
TEST_F(DefaultSearchManagerTest, ReadAndWritePref) {
  DefaultSearchManager manager(pref_service());
  TemplateURLData data;
  data.short_name = base::UTF8ToUTF16("name1");
  data.SetKeyword(base::UTF8ToUTF16("key1"));
  data.SetURL("http://foo1/{searchTerms}");
  data.suggestions_url = "http://sugg1";
  data.alternate_urls.push_back("http://foo1/alt");
  data.favicon_url = GURL("http://icon1");
  data.safe_for_autoreplace = true;
  data.show_in_default_list = true;
  base::SplitString("UTF-8;UTF-16", ';', &data.input_encodings);
  data.date_created = base::Time();
  data.last_modified = base::Time();

  manager.SetUserSelectedDefaultSearchEngine(data);
  TemplateURLData read_data;
  manager.GetDefaultSearchEngine(&read_data);
  ExpectSimilar(&data, &read_data);
}

// Test that there is no default value set in the pref.
TEST_F(DefaultSearchManagerTest, ReadDefaultPref) {
  DefaultSearchManager manager(pref_service());
  TemplateURLData read_data;
  EXPECT_FALSE(manager.GetDefaultSearchEngine(&read_data));
}
