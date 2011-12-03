// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/browser/webdata/web_database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

class KeywordTableTest : public testing::Test {
 public:
  KeywordTableTest() {}
  virtual ~KeywordTableTest() {}

 protected:
  virtual void SetUp() {
    PathService::Get(chrome::DIR_TEST_DATA, &file_);
    const std::string test_db = "TestWebDatabase" +
        base::Int64ToString(Time::Now().ToTimeT()) +
        ".db";
    file_ = file_.AppendASCII(test_db);
    file_util::Delete(file_, false);
  }

  virtual void TearDown() {
    file_util::Delete(file_, false);
  }

  static int64 GetID(const TemplateURL* url) {
    return url->id();
  }

  static void SetID(int64 new_id, TemplateURL* url) {
    url->set_id(new_id);
  }

  static void set_prepopulate_id(TemplateURL* url, int id) {
    url->SetPrepopulateId(id);
  }

  static void set_logo_id(TemplateURL* url, int id) {
    url->set_logo_id(id);
  }

  FilePath file_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeywordTableTest);
};


TEST_F(KeywordTableTest, Keywords) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  TemplateURL template_url;
  template_url.set_short_name(ASCIIToUTF16("short_name"));
  template_url.set_keyword(ASCIIToUTF16("keyword"));
  GURL favicon_url("http://favicon.url/");
  GURL originating_url("http://google.com/");
  template_url.SetFaviconURL(favicon_url);
  template_url.SetURL("http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  Time created_time = Time::Now();
  template_url.set_date_created(created_time);
  Time last_modified_time = created_time + TimeDelta::FromSeconds(10);
  template_url.set_last_modified(last_modified_time);
  template_url.set_show_in_default_list(true);
  template_url.set_originating_url(originating_url);
  template_url.set_usage_count(32);
  template_url.add_input_encoding("UTF-8");
  template_url.add_input_encoding("UTF-16");
  set_prepopulate_id(&template_url, 10);
  set_logo_id(&template_url, 1000);
  template_url.set_created_by_policy(true);
  template_url.SetInstantURL("http://instant/", 0, 0);
  SetID(1, &template_url);

  EXPECT_TRUE(db.GetKeywordTable()->AddKeyword(template_url));

  std::vector<TemplateURL*> template_urls;
  EXPECT_TRUE(db.GetKeywordTable()->GetKeywords(&template_urls));

  EXPECT_EQ(1U, template_urls.size());
  const TemplateURL* restored_url = template_urls.front();

  EXPECT_EQ(template_url.short_name(), restored_url->short_name());

  EXPECT_EQ(template_url.keyword(), restored_url->keyword());

  EXPECT_FALSE(restored_url->autogenerate_keyword());

  EXPECT_TRUE(favicon_url == restored_url->GetFaviconURL());

  EXPECT_TRUE(restored_url->safe_for_autoreplace());

  // The database stores time only at the resolution of a second.
  EXPECT_EQ(created_time.ToTimeT(), restored_url->date_created().ToTimeT());

  EXPECT_EQ(last_modified_time.ToTimeT(),
            restored_url->last_modified().ToTimeT());

  EXPECT_TRUE(restored_url->show_in_default_list());

  EXPECT_EQ(GetID(&template_url), GetID(restored_url));

  EXPECT_TRUE(originating_url == restored_url->originating_url());

  EXPECT_EQ(32, restored_url->usage_count());

  ASSERT_EQ(2U, restored_url->input_encodings().size());
  EXPECT_EQ("UTF-8", restored_url->input_encodings()[0]);
  EXPECT_EQ("UTF-16", restored_url->input_encodings()[1]);

  EXPECT_EQ(10, restored_url->prepopulate_id());

  EXPECT_EQ(1000, restored_url->logo_id());

  EXPECT_TRUE(restored_url->created_by_policy());

  ASSERT_TRUE(restored_url->instant_url());
  EXPECT_EQ("http://instant/", restored_url->instant_url()->url());

  EXPECT_TRUE(db.GetKeywordTable()->RemoveKeyword(restored_url->id()));

  template_urls.clear();
  EXPECT_TRUE(db.GetKeywordTable()->GetKeywords(&template_urls));

  EXPECT_EQ(0U, template_urls.size());

  delete restored_url;
}

TEST_F(KeywordTableTest, KeywordMisc) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  ASSERT_EQ(0, db.GetKeywordTable()->GetDefaultSearchProviderID());
  ASSERT_EQ(0, db.GetKeywordTable()->GetBuiltinKeywordVersion());

  TemplateURL template_url;
  template_url.set_short_name(ASCIIToUTF16("short_name"));
  template_url.set_keyword(ASCIIToUTF16("keyword"));
  GURL favicon_url("http://favicon.url/");
  GURL originating_url("http://google.com/");
  template_url.SetFaviconURL(favicon_url);
  template_url.SetURL("http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  Time created_time = Time::Now();
  template_url.set_date_created(created_time);
  Time last_modified_time = created_time + TimeDelta::FromSeconds(10);
  template_url.set_last_modified(last_modified_time);
  template_url.set_show_in_default_list(true);
  template_url.set_originating_url(originating_url);
  template_url.set_usage_count(32);
  template_url.add_input_encoding("UTF-8");
  template_url.add_input_encoding("UTF-16");
  set_prepopulate_id(&template_url, 10);
  set_logo_id(&template_url, 1000);
  template_url.set_created_by_policy(true);
  template_url.SetInstantURL("http://instant/", 0, 0);
  SetID(10, &template_url);
  ASSERT_TRUE(db.GetKeywordTable()->AddKeyword(template_url));

  ASSERT_TRUE(db.GetKeywordTable()->SetDefaultSearchProviderID(10));
  ASSERT_TRUE(db.GetKeywordTable()->SetBuiltinKeywordVersion(11));

  ASSERT_EQ(10, db.GetKeywordTable()->GetDefaultSearchProviderID());
  ASSERT_EQ(11, db.GetKeywordTable()->GetBuiltinKeywordVersion());
}

TEST_F(KeywordTableTest, DefaultSearchProviderBackup) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  EXPECT_EQ(0, db.GetKeywordTable()->GetDefaultSearchProviderID());

  TemplateURL template_url;
  template_url.set_short_name(ASCIIToUTF16("short_name"));
  template_url.set_keyword(ASCIIToUTF16("keyword"));
  GURL favicon_url("http://favicon.url/");
  GURL originating_url("http://originating.url/");
  template_url.SetFaviconURL(favicon_url);
  template_url.SetURL("http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  template_url.set_show_in_default_list(true);
  template_url.SetSuggestionsURL("url2", 0, 0);
  SetID(1, &template_url);

  EXPECT_TRUE(db.GetKeywordTable()->AddKeyword(template_url));

  ASSERT_TRUE(db.GetKeywordTable()->SetDefaultSearchProviderID(1));
  EXPECT_TRUE(db.GetKeywordTable()->IsBackupSignatureValid());
  EXPECT_EQ(1, db.GetKeywordTable()->GetDefaultSearchProviderID());
  EXPECT_EQ(1, db.GetKeywordTable()->GetDefaultSearchProviderIDBackup());
  EXPECT_FALSE(db.GetKeywordTable()->DidDefaultSearchProviderChange());

  // Change the actual setting.
  ASSERT_TRUE(db.GetKeywordTable()->meta_table_->SetValue(
      "Default Search Provider ID", 2));
  EXPECT_TRUE(db.GetKeywordTable()->IsBackupSignatureValid());
  EXPECT_EQ(2, db.GetKeywordTable()->GetDefaultSearchProviderID());
  EXPECT_EQ(1, db.GetKeywordTable()->GetDefaultSearchProviderIDBackup());
  EXPECT_TRUE(db.GetKeywordTable()->DidDefaultSearchProviderChange());

  // Change the backup.
  ASSERT_TRUE(db.GetKeywordTable()->meta_table_->SetValue(
      "Default Search Provider ID", 1));
  ASSERT_TRUE(db.GetKeywordTable()->meta_table_->SetValue(
      "Default Search Provider ID Backup", 2));
  EXPECT_FALSE(db.GetKeywordTable()->IsBackupSignatureValid());
  EXPECT_EQ(1, db.GetKeywordTable()->GetDefaultSearchProviderID());
  EXPECT_EQ(0, db.GetKeywordTable()->GetDefaultSearchProviderIDBackup());
  EXPECT_TRUE(db.GetKeywordTable()->DidDefaultSearchProviderChange());

  // Change the signature.
  ASSERT_TRUE(db.GetKeywordTable()->meta_table_->SetValue(
      "Default Search Provider ID Backup", 1));
  ASSERT_TRUE(db.GetKeywordTable()->meta_table_->SetValue(
      "Default Search Provider ID Backup Signature", ""));
  EXPECT_FALSE(db.GetKeywordTable()->IsBackupSignatureValid());
  EXPECT_EQ(1, db.GetKeywordTable()->GetDefaultSearchProviderID());
  EXPECT_EQ(0, db.GetKeywordTable()->GetDefaultSearchProviderIDBackup());
  EXPECT_TRUE(db.GetKeywordTable()->DidDefaultSearchProviderChange());

  // Change keywords.
  ASSERT_TRUE(db.GetKeywordTable()->UpdateBackupSignature());
  sql::Statement remove_keyword(db.GetKeywordTable()->db_->GetUniqueStatement(
      "DELETE FROM keywords WHERE id=1"));
  ASSERT_TRUE(remove_keyword.Run());
  EXPECT_FALSE(db.GetKeywordTable()->IsBackupSignatureValid());
  EXPECT_EQ(1, db.GetKeywordTable()->GetDefaultSearchProviderID());
  EXPECT_EQ(0, db.GetKeywordTable()->GetDefaultSearchProviderIDBackup());
  EXPECT_TRUE(db.GetKeywordTable()->DidDefaultSearchProviderChange());
}

TEST_F(KeywordTableTest, UpdateKeyword) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  TemplateURL template_url;
  template_url.set_short_name(ASCIIToUTF16("short_name"));
  template_url.set_keyword(ASCIIToUTF16("keyword"));
  GURL favicon_url("http://favicon.url/");
  GURL originating_url("http://originating.url/");
  template_url.SetFaviconURL(favicon_url);
  template_url.SetURL("http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  template_url.set_show_in_default_list(true);
  template_url.SetSuggestionsURL("url2", 0, 0);
  SetID(1, &template_url);

  EXPECT_TRUE(db.GetKeywordTable()->AddKeyword(template_url));

  GURL originating_url2("http://originating.url/");
  template_url.set_originating_url(originating_url2);
  template_url.set_autogenerate_keyword(true);
  EXPECT_EQ(ASCIIToUTF16("url"), template_url.keyword());
  template_url.add_input_encoding("Shift_JIS");
  set_prepopulate_id(&template_url, 5);
  set_logo_id(&template_url, 2000);
  template_url.SetInstantURL("http://instant2/", 0, 0);
  EXPECT_TRUE(db.GetKeywordTable()->UpdateKeyword(template_url));

  std::vector<TemplateURL*> template_urls;
  EXPECT_TRUE(db.GetKeywordTable()->GetKeywords(&template_urls));

  EXPECT_EQ(1U, template_urls.size());
  const TemplateURL* restored_url = template_urls.front();

  EXPECT_EQ(template_url.short_name(), restored_url->short_name());

  EXPECT_EQ(template_url.keyword(), restored_url->keyword());

  EXPECT_TRUE(restored_url->autogenerate_keyword());

  EXPECT_TRUE(favicon_url == restored_url->GetFaviconURL());

  EXPECT_TRUE(restored_url->safe_for_autoreplace());

  EXPECT_TRUE(restored_url->show_in_default_list());

  EXPECT_EQ(GetID(&template_url), GetID(restored_url));

  EXPECT_TRUE(originating_url2 == restored_url->originating_url());

  ASSERT_EQ(1U, restored_url->input_encodings().size());
  ASSERT_EQ("Shift_JIS", restored_url->input_encodings()[0]);

  EXPECT_EQ(template_url.suggestions_url()->url(),
            restored_url->suggestions_url()->url());

  EXPECT_EQ(template_url.id(), restored_url->id());

  EXPECT_EQ(template_url.prepopulate_id(), restored_url->prepopulate_id());

  EXPECT_EQ(template_url.logo_id(), restored_url->logo_id());

  EXPECT_TRUE(restored_url->instant_url());
  EXPECT_EQ(template_url.instant_url()->url(),
            restored_url->instant_url()->url());

  delete restored_url;
}

TEST_F(KeywordTableTest, KeywordWithNoFavicon) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  TemplateURL template_url;
  template_url.set_short_name(ASCIIToUTF16("short_name"));
  template_url.set_keyword(ASCIIToUTF16("keyword"));
  template_url.SetURL("http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  SetID(-100, &template_url);

  EXPECT_TRUE(db.GetKeywordTable()->AddKeyword(template_url));

  std::vector<TemplateURL*> template_urls;
  EXPECT_TRUE(db.GetKeywordTable()->GetKeywords(&template_urls));
  EXPECT_EQ(1U, template_urls.size());
  const TemplateURL* restored_url = template_urls.front();

  EXPECT_EQ(template_url.short_name(), restored_url->short_name());
  EXPECT_EQ(template_url.keyword(), restored_url->keyword());
  EXPECT_TRUE(!restored_url->GetFaviconURL().is_valid());
  EXPECT_TRUE(restored_url->safe_for_autoreplace());
  EXPECT_EQ(GetID(&template_url), GetID(restored_url));
  delete restored_url;
}
