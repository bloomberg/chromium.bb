// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/default_search_pref_migration.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class DefaultSearchPrefMigrationTest : public testing::Test {
 public:
  DefaultSearchPrefMigrationTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;

  scoped_ptr<TemplateURL> CreateKeyword(const std::string& short_name,
                                        const std::string& keyword,
                                        const std::string& url);

  TestingProfile* profile() { return profile_.get(); }

  DefaultSearchManager* default_search_manager() {
    return default_search_manager_.get();
  }

 private:
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<DefaultSearchManager> default_search_manager_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchPrefMigrationTest);
};

DefaultSearchPrefMigrationTest::DefaultSearchPrefMigrationTest() {
}

void DefaultSearchPrefMigrationTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  profile_.reset(new TestingProfile(temp_dir_.path()));
  default_search_manager_.reset(new DefaultSearchManager(
      profile_->GetPrefs(), DefaultSearchManager::ObserverCallback()));
}

scoped_ptr<TemplateURL> DefaultSearchPrefMigrationTest::CreateKeyword(
    const std::string& short_name,
    const std::string& keyword,
    const std::string& url) {
  TemplateURLData data;
  data.short_name = base::ASCIIToUTF16(short_name);
  data.SetKeyword(base::ASCIIToUTF16(keyword));
  data.SetURL(url);
  scoped_ptr<TemplateURL> t_url(new TemplateURL(data));
  return t_url.Pass();
}

TEST_F(DefaultSearchPrefMigrationTest, MigrateUserSelectedValue) {
  scoped_ptr<TemplateURL> t_url(
      CreateKeyword("name1", "key1", "http://foo1/{searchTerms}"));
  // Store a value in the legacy location.
  TemplateURLService::SaveDefaultSearchProviderToPrefs(t_url.get(),
                                                       profile()->GetPrefs());

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(profile()->GetPrefs());

  // Test that it was migrated.
  DefaultSearchManager::Source source;
  const TemplateURLData* modern_default =
      default_search_manager()->GetDefaultSearchEngine(&source);
  ASSERT_TRUE(modern_default);
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
  EXPECT_EQ(t_url->short_name(), modern_default->short_name);
  EXPECT_EQ(t_url->keyword(), modern_default->keyword());
  EXPECT_EQ(t_url->url(), modern_default->url());
}

TEST_F(DefaultSearchPrefMigrationTest, MigrateOnlyOnce) {
  scoped_ptr<TemplateURL> t_url(
      CreateKeyword("name1", "key1", "http://foo1/{searchTerms}"));
  // Store a value in the legacy location.
  TemplateURLService::SaveDefaultSearchProviderToPrefs(t_url.get(),
                                                       profile()->GetPrefs());

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(profile()->GetPrefs());

  // Test that it was migrated.
  DefaultSearchManager::Source source;
  const TemplateURLData* modern_default =
      default_search_manager()->GetDefaultSearchEngine(&source);
  ASSERT_TRUE(modern_default);
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
  EXPECT_EQ(t_url->short_name(), modern_default->short_name);
  EXPECT_EQ(t_url->keyword(), modern_default->keyword());
  EXPECT_EQ(t_url->url(), modern_default->url());
  default_search_manager()->ClearUserSelectedDefaultSearchEngine();

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(profile()->GetPrefs());

  // Test that it was NOT migrated.
  modern_default = default_search_manager()->GetDefaultSearchEngine(&source);
  ASSERT_TRUE(modern_default);
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
}

TEST_F(DefaultSearchPrefMigrationTest, ModernValuePresent) {
  scoped_ptr<TemplateURL> t_url(
      CreateKeyword("name1", "key1", "http://foo1/{searchTerms}"));
  scoped_ptr<TemplateURL> t_url2(
      CreateKeyword("name2", "key2", "http://foo2/{searchTerms}"));
  // Store a value in the legacy location.
  TemplateURLService::SaveDefaultSearchProviderToPrefs(t_url.get(),
                                                       profile()->GetPrefs());

  // Store another value in the modern location.
  default_search_manager()->SetUserSelectedDefaultSearchEngine(t_url2->data());

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(profile()->GetPrefs());

  // Test that no migration occurred. The modern value is left intact.
  DefaultSearchManager::Source source;
  const TemplateURLData* modern_default =
      default_search_manager()->GetDefaultSearchEngine(&source);
  ASSERT_TRUE(modern_default);
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
  EXPECT_EQ(t_url2->short_name(), modern_default->short_name);
  EXPECT_EQ(t_url2->keyword(), modern_default->keyword());
  EXPECT_EQ(t_url2->url(), modern_default->url());
}

TEST_F(DefaultSearchPrefMigrationTest,
       AutomaticallySelectedValueIsNotMigrated) {
  DefaultSearchManager::Source source;
  TemplateURLData prepopulated_default(
      *default_search_manager()->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);

  TemplateURL prepopulated_turl(prepopulated_default);

  // Store a value in the legacy location.
  TemplateURLService::SaveDefaultSearchProviderToPrefs(&prepopulated_turl,
                                                       profile()->GetPrefs());

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(profile()->GetPrefs());

  // Test that the legacy value is not migrated, as it is not user-selected.
  default_search_manager()->GetDefaultSearchEngine(&source);
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
}
