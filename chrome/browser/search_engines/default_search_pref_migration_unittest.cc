// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/default_search_pref_migration.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class DefaultSearchPrefMigrationTest : public testing::Test {
 public:
  DefaultSearchPrefMigrationTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  scoped_ptr<TemplateURL> CreateKeyword(const std::string& short_name,
                                        const std::string& keyword,
                                        const std::string& url);

  TemplateURLServiceTestUtil* test_util() { return &test_util_; }

 private:
  TemplateURLServiceTestUtil test_util_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchPrefMigrationTest);
};

DefaultSearchPrefMigrationTest::DefaultSearchPrefMigrationTest() {
}

void DefaultSearchPrefMigrationTest::SetUp() {
  test_util_.SetUp();
}

void DefaultSearchPrefMigrationTest::TearDown() {
  test_util_.TearDown();
}

scoped_ptr<TemplateURL> DefaultSearchPrefMigrationTest::CreateKeyword(
    const std::string& short_name,
    const std::string& keyword,
    const std::string& url) {
  TemplateURLData data;
  data.short_name = base::ASCIIToUTF16(short_name);
  data.SetKeyword(base::ASCIIToUTF16(keyword));
  data.SetURL(url);
  scoped_ptr<TemplateURL> t_url(new TemplateURL(test_util_.profile(), data));
  return t_url.Pass();
}

TEST_F(DefaultSearchPrefMigrationTest, MigrateUserSelectedValue) {
  scoped_ptr<TemplateURL> t_url(
      CreateKeyword("name1", "key1", "http://foo1/{searchTerms}"));
  // Store a value in the legacy location.
  TemplateURLService::SaveDefaultSearchProviderToPrefs(
      t_url.get(), test_util()->profile()->GetPrefs());

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(
      test_util()->profile()->GetPrefs());

  // Test that it was migrated.
  TemplateURLData modern_default;
  ASSERT_TRUE(DefaultSearchManager(test_util()->profile()->GetPrefs())
                  .GetDefaultSearchEngine(&modern_default));
  EXPECT_EQ(t_url->short_name(), modern_default.short_name);
  EXPECT_EQ(t_url->keyword(), modern_default.keyword());
  EXPECT_EQ(t_url->url(), modern_default.url());
}

TEST_F(DefaultSearchPrefMigrationTest, ModernValuePresent) {
  scoped_ptr<TemplateURL> t_url(
      CreateKeyword("name1", "key1", "http://foo1/{searchTerms}"));
  scoped_ptr<TemplateURL> t_url2(
      CreateKeyword("name2", "key2", "http://foo2/{searchTerms}"));
  // Store a value in the legacy location.
  TemplateURLService::SaveDefaultSearchProviderToPrefs(
      t_url.get(), test_util()->profile()->GetPrefs());

  // Store another value in the modern location.
  DefaultSearchManager(test_util()->profile()->GetPrefs())
      .SetUserSelectedDefaultSearchEngine(t_url2->data());

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(
      test_util()->profile()->GetPrefs());

  // Test that no migration occurred. The modern value is left intact.
  TemplateURLData modern_default;
  ASSERT_TRUE(DefaultSearchManager(test_util()->profile()->GetPrefs())
                  .GetDefaultSearchEngine(&modern_default));
  EXPECT_EQ(t_url2->short_name(), modern_default.short_name);
  EXPECT_EQ(t_url2->keyword(), modern_default.keyword());
  EXPECT_EQ(t_url2->url(), modern_default.url());
}

TEST_F(DefaultSearchPrefMigrationTest,
       AutomaticallySelectedValueIsNotMigrated) {
  test_util()->VerifyLoad();
  scoped_ptr<TemplateURLData> legacy_default;
  bool legacy_is_managed = false;
  // The initialization of the TemplateURLService will have stored the
  // pre-populated DSE in the legacy location in prefs.
  ASSERT_TRUE(TemplateURLService::LoadDefaultSearchProviderFromPrefs(
      test_util()->profile()->GetPrefs(), &legacy_default, &legacy_is_managed));
  EXPECT_FALSE(legacy_is_managed);
  EXPECT_TRUE(legacy_default);
  EXPECT_GT(legacy_default->prepopulate_id, 0);

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(
      test_util()->profile()->GetPrefs());

  // Test that the legacy value is not migrated, as it is not user-selected.
  TemplateURLData modern_default;
  ASSERT_FALSE(DefaultSearchManager(test_util()->profile()->GetPrefs())
                   .GetDefaultSearchEngine(&modern_default));
}

TEST_F(DefaultSearchPrefMigrationTest, ManagedValueIsNotMigrated) {
  // Set a managed preference that establishes a default search provider.
  const char kName[] = "test1";
  const char kKeyword[] = "test.com";
  const char kSearchURL[] = "http://test.com/search?t={searchTerms}";
  const char kIconURL[] = "http://test.com/icon.jpg";
  const char kEncodings[] = "UTF-16;UTF-32";
  const char kAlternateURL[] = "http://test.com/search#t={searchTerms}";
  const char kSearchTermsReplacementKey[] = "espv";

  // This method only updates the legacy location for managed DSEs. So it will
  // not cause DefaultSearchManager to report a value.
  test_util()->SetManagedDefaultSearchPreferences(true,
                                                  kName,
                                                  kKeyword,
                                                  kSearchURL,
                                                  std::string(),
                                                  kIconURL,
                                                  kEncodings,
                                                  kAlternateURL,
                                                  kSearchTermsReplacementKey);
  test_util()->VerifyLoad();

  // Verify that the policy value is correctly installed.
  scoped_ptr<TemplateURLData> legacy_default;
  bool legacy_is_managed = false;
  ASSERT_TRUE(TemplateURLService::LoadDefaultSearchProviderFromPrefs(
      test_util()->profile()->GetPrefs(), &legacy_default, &legacy_is_managed));
  EXPECT_TRUE(legacy_is_managed);
  EXPECT_TRUE(legacy_default);

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(
      test_util()->profile()->GetPrefs());

  // Test that the policy-defined value is not migrated.
  TemplateURLData modern_default;
  ASSERT_FALSE(DefaultSearchManager(test_util()->profile()->GetPrefs())
                   .GetDefaultSearchEngine(&modern_default));
}
