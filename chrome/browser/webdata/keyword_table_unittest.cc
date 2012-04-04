// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
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

  static void SetID(int64 new_id, TemplateURL* url) {
    url->set_id(new_id);
  }

  FilePath file_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeywordTableTest);
};


TEST_F(KeywordTableTest, Keywords) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));
  KeywordTable* keyword_table = db.GetKeywordTable();

  TemplateURL keyword;
  keyword.set_short_name(ASCIIToUTF16("short_name"));
  keyword.set_originating_url(GURL("http://google.com/"));
  keyword.set_keyword(ASCIIToUTF16("keyword"));
  keyword.set_show_in_default_list(true);
  keyword.set_safe_for_autoreplace(true);
  keyword.add_input_encoding("UTF-8");
  keyword.add_input_encoding("UTF-16");
  SetID(1, &keyword);
  keyword.set_date_created(Time::Now());
  keyword.set_last_modified(
      keyword.date_created() + TimeDelta::FromSeconds(10));
  keyword.set_created_by_policy(true);
  keyword.set_usage_count(32);
  keyword.SetPrepopulateId(10);
  keyword.SetURL("http://url/");
  keyword.SetInstantURL("http://instant/");
  keyword.set_favicon_url(GURL("http://favicon.url/"));
  EXPECT_TRUE(keyword_table->AddKeyword(keyword));

  KeywordTable::Keywords keywords;
  EXPECT_TRUE(keyword_table->GetKeywords(&keywords));
  EXPECT_EQ(1U, keywords.size());
  const TemplateURL* restored_keyword = keywords.front();

  EXPECT_EQ(keyword.short_name(), restored_keyword->short_name());
  EXPECT_EQ(keyword.originating_url(), restored_keyword->originating_url());
  EXPECT_EQ(keyword.autogenerate_keyword(),
            restored_keyword->autogenerate_keyword());
  EXPECT_EQ(keyword.keyword(), restored_keyword->keyword());
  EXPECT_EQ(keyword.show_in_default_list(),
            restored_keyword->show_in_default_list());
  EXPECT_EQ(keyword.safe_for_autoreplace(),
            restored_keyword->safe_for_autoreplace());
  EXPECT_EQ(keyword.input_encodings(), restored_keyword->input_encodings());
  EXPECT_EQ(keyword.id(), restored_keyword->id());
  // The database stores time only at the resolution of a second.
  EXPECT_EQ(keyword.date_created().ToTimeT(),
            restored_keyword->date_created().ToTimeT());
  EXPECT_EQ(keyword.last_modified().ToTimeT(),
            restored_keyword->last_modified().ToTimeT());
  EXPECT_EQ(keyword.created_by_policy(), restored_keyword->created_by_policy());
  EXPECT_EQ(keyword.usage_count(), restored_keyword->usage_count());
  EXPECT_EQ(keyword.prepopulate_id(), restored_keyword->prepopulate_id());
  ASSERT_TRUE(restored_keyword->url());
  EXPECT_EQ(keyword.url()->url(), restored_keyword->url()->url());
  ASSERT_TRUE(restored_keyword->instant_url());
  EXPECT_EQ(keyword.instant_url()->url(),
            restored_keyword->instant_url()->url());
  EXPECT_EQ(keyword.favicon_url(), restored_keyword->favicon_url());

  EXPECT_TRUE(keyword_table->RemoveKeyword(restored_keyword->id()));
  STLDeleteElements(&keywords);

  KeywordTable::Keywords empty_keywords;
  EXPECT_TRUE(keyword_table->GetKeywords(&empty_keywords));
  EXPECT_EQ(0U, empty_keywords.size());
  STLDeleteElements(&empty_keywords);
}

TEST_F(KeywordTableTest, KeywordMisc) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));
  KeywordTable* keyword_table = db.GetKeywordTable();

  ASSERT_EQ(kInvalidTemplateURLID, keyword_table->GetDefaultSearchProviderID());
  ASSERT_EQ(0, keyword_table->GetBuiltinKeywordVersion());

  TemplateURL keyword;
  keyword.set_short_name(ASCIIToUTF16("short_name"));
  keyword.set_originating_url(GURL("http://google.com/"));
  keyword.set_keyword(ASCIIToUTF16("keyword"));
  keyword.set_show_in_default_list(true);
  keyword.set_safe_for_autoreplace(true);
  keyword.add_input_encoding("UTF-8");
  keyword.add_input_encoding("UTF-16");
  SetID(10, &keyword);
  keyword.set_date_created(Time::Now());
  keyword.set_last_modified(
      keyword.date_created() + TimeDelta::FromSeconds(10));
  keyword.set_created_by_policy(true);
  keyword.set_usage_count(32);
  keyword.SetPrepopulateId(10);
  keyword.SetURL("http://url/");
  keyword.SetInstantURL("http://instant/");
  keyword.set_favicon_url(GURL("http://favicon.url/"));
  ASSERT_TRUE(keyword_table->AddKeyword(keyword));

  ASSERT_TRUE(keyword_table->SetDefaultSearchProviderID(10));
  ASSERT_TRUE(keyword_table->SetBuiltinKeywordVersion(11));

  ASSERT_EQ(10, keyword_table->GetDefaultSearchProviderID());
  ASSERT_EQ(11, keyword_table->GetBuiltinKeywordVersion());
}

TEST_F(KeywordTableTest, DefaultSearchProviderBackup) {
  // TODO(ivankr): suppress keyword_table.cc ERROR logs.
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));
  KeywordTable* keyword_table = db.GetKeywordTable();

  EXPECT_EQ(kInvalidTemplateURLID, keyword_table->GetDefaultSearchProviderID());

  TemplateURL keyword;
  keyword.set_short_name(ASCIIToUTF16("short_name"));
  keyword.set_keyword(ASCIIToUTF16("keyword"));
  keyword.set_show_in_default_list(true);
  keyword.set_safe_for_autoreplace(true);
  SetID(1, &keyword);
  keyword.SetSuggestionsURL("url2");
  keyword.SetURL("http://url/");
  keyword.set_favicon_url(GURL("http://favicon.url/"));
  EXPECT_TRUE(keyword_table->AddKeyword(keyword));

  ASSERT_TRUE(keyword_table->SetDefaultSearchProviderID(1));
  EXPECT_TRUE(keyword_table->IsBackupSignatureValid());
  EXPECT_EQ(1, keyword_table->GetDefaultSearchProviderID());

  scoped_ptr<TemplateURL> backup_url(
      keyword_table->GetDefaultSearchProviderBackup());
  // Backup URL should have an invalid ID.
  EXPECT_EQ(kInvalidTemplateURLID, backup_url->id());
  EXPECT_EQ(keyword.short_name(), backup_url->short_name());
  EXPECT_EQ(keyword.keyword(), backup_url->keyword());
  EXPECT_EQ(keyword.favicon_url(), backup_url->favicon_url());
  ASSERT_TRUE(backup_url->url());
  EXPECT_EQ(keyword.url()->url(), backup_url->url()->url());
  EXPECT_EQ(keyword.safe_for_autoreplace(), backup_url->safe_for_autoreplace());
  EXPECT_EQ(keyword.show_in_default_list(), backup_url->show_in_default_list());
  ASSERT_TRUE(backup_url->suggestions_url());
  EXPECT_EQ(keyword.suggestions_url()->url(),
            backup_url->suggestions_url()->url());
  EXPECT_FALSE(keyword_table->DidDefaultSearchProviderChange());

  // Change the actual setting.
  ASSERT_TRUE(keyword_table->meta_table_->SetValue(
      KeywordTable::kDefaultSearchProviderKey, 2));
  EXPECT_TRUE(keyword_table->IsBackupSignatureValid());
  EXPECT_EQ(2, keyword_table->GetDefaultSearchProviderID());

  backup_url.reset(keyword_table->GetDefaultSearchProviderBackup());
  EXPECT_EQ(kInvalidTemplateURLID, backup_url->id());
  EXPECT_EQ(keyword.short_name(), backup_url->short_name());
  EXPECT_EQ(keyword.keyword(), backup_url->keyword());
  EXPECT_EQ(keyword.favicon_url(), backup_url->favicon_url());
  ASSERT_TRUE(backup_url->url());
  EXPECT_EQ(keyword.url()->url(), backup_url->url()->url());
  EXPECT_EQ(keyword.safe_for_autoreplace(), backup_url->safe_for_autoreplace());
  EXPECT_EQ(keyword.show_in_default_list(), backup_url->show_in_default_list());
  ASSERT_TRUE(backup_url->suggestions_url());
  EXPECT_EQ(keyword.suggestions_url()->url(),
            backup_url->suggestions_url()->url());
  EXPECT_TRUE(keyword_table->DidDefaultSearchProviderChange());

  // Change the backup.
  ASSERT_TRUE(keyword_table->meta_table_->SetValue(
      KeywordTable::kDefaultSearchProviderKey, 1));
  ASSERT_TRUE(keyword_table->meta_table_->SetValue(
      KeywordTable::kDefaultSearchIDBackupKey, 2));
  EXPECT_FALSE(keyword_table->IsBackupSignatureValid());
  EXPECT_EQ(1, keyword_table->GetDefaultSearchProviderID());
  EXPECT_EQ(NULL, keyword_table->GetDefaultSearchProviderBackup());
  EXPECT_TRUE(keyword_table->DidDefaultSearchProviderChange());

  // Change the signature.
  ASSERT_TRUE(keyword_table->meta_table_->SetValue(
      KeywordTable::kDefaultSearchIDBackupKey, 1));
  ASSERT_TRUE(keyword_table->meta_table_->SetValue(
      KeywordTable::kBackupSignatureKey, std::string()));
  EXPECT_FALSE(keyword_table->IsBackupSignatureValid());
  EXPECT_EQ(1, keyword_table->GetDefaultSearchProviderID());
  EXPECT_EQ(NULL, keyword_table->GetDefaultSearchProviderBackup());
  EXPECT_TRUE(keyword_table->DidDefaultSearchProviderChange());

  // Change keywords.
  ASSERT_TRUE(keyword_table->UpdateBackupSignature());
  sql::Statement remove_keyword(keyword_table->db_->GetUniqueStatement(
      "DELETE FROM keywords WHERE id=1"));
  ASSERT_TRUE(remove_keyword.Run());
  EXPECT_TRUE(keyword_table->IsBackupSignatureValid());
  EXPECT_EQ(1, keyword_table->GetDefaultSearchProviderID());

  backup_url.reset(keyword_table->GetDefaultSearchProviderBackup());
  EXPECT_EQ(kInvalidTemplateURLID, backup_url->id());
  EXPECT_EQ(keyword.short_name(), backup_url->short_name());
  EXPECT_EQ(keyword.keyword(), backup_url->keyword());
  EXPECT_EQ(keyword.favicon_url(), backup_url->favicon_url());
  ASSERT_TRUE(backup_url->url());
  EXPECT_EQ(keyword.url()->url(), backup_url->url()->url());
  EXPECT_EQ(keyword.safe_for_autoreplace(), backup_url->safe_for_autoreplace());
  EXPECT_EQ(keyword.show_in_default_list(), backup_url->show_in_default_list());
  ASSERT_TRUE(backup_url->suggestions_url());
  EXPECT_EQ(keyword.suggestions_url()->url(),
            backup_url->suggestions_url()->url());
  EXPECT_TRUE(keyword_table->DidDefaultSearchProviderChange());

  // Change keywords backup.
  sql::Statement remove_keyword_backup(keyword_table->db_->GetUniqueStatement(
          "DELETE FROM keywords_backup WHERE id=1"));
  ASSERT_TRUE(remove_keyword_backup.Run());
  EXPECT_FALSE(keyword_table->IsBackupSignatureValid());
  EXPECT_EQ(1, keyword_table->GetDefaultSearchProviderID());
  EXPECT_EQ(NULL, keyword_table->GetDefaultSearchProviderBackup());
  EXPECT_TRUE(keyword_table->DidDefaultSearchProviderChange());
}

TEST_F(KeywordTableTest, GetTableContents) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));
  KeywordTable* keyword_table = db.GetKeywordTable();

  TemplateURL keyword;
  keyword.set_short_name(ASCIIToUTF16("short_name"));
  keyword.set_keyword(ASCIIToUTF16("keyword"));
  keyword.set_show_in_default_list(true);
  keyword.set_safe_for_autoreplace(true);
  SetID(1, &keyword);
  keyword.set_date_created(base::Time::UnixEpoch());
  keyword.set_last_modified(base::Time::UnixEpoch());
  keyword.set_sync_guid("1234-5678-90AB-CDEF");
  keyword.SetSuggestionsURL("url2");
  keyword.SetURL("http://url/");
  keyword.set_favicon_url(GURL("http://favicon.url/"));
  ASSERT_TRUE(keyword_table->AddKeyword(keyword));

  keyword.set_originating_url(GURL("http://originating.url/"));
  keyword.set_autogenerate_keyword(true);
  EXPECT_EQ(ASCIIToUTF16("url"), keyword.keyword());
  keyword.add_input_encoding("Shift_JIS");
  SetID(2, &keyword);
  keyword.SetPrepopulateId(5);
  keyword.set_sync_guid("FEDC-BA09-8765-4321");
  keyword.SetInstantURL("http://instant2/");
  ASSERT_TRUE(keyword_table->AddKeyword(keyword));

  const char kTestContents[] = "1short_namekeywordhttp://favicon.url/"
      "http://url/1001url2000001234-5678-90AB-CDEF2short_nameurl"
      "http://favicon.url/http://url/1http://originating.url/00Shift_JIS1url251"
      "00http://instant2/0FEDC-BA09-8765-4321";

  std::string contents;
  ASSERT_TRUE(keyword_table->GetTableContents("keywords", &contents));
  EXPECT_EQ(kTestContents, contents);

  ASSERT_TRUE(keyword_table->GetTableContents("keywords_backup", &contents));
  EXPECT_EQ(kTestContents, contents);
}

TEST_F(KeywordTableTest, GetTableContentsOrdering) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));
  KeywordTable* keyword_table = db.GetKeywordTable();

  TemplateURL keyword;
  keyword.set_short_name(ASCIIToUTF16("short_name"));
  keyword.set_keyword(ASCIIToUTF16("keyword"));
  keyword.set_show_in_default_list(true);
  keyword.set_safe_for_autoreplace(true);
  SetID(2, &keyword);
  keyword.set_date_created(base::Time::UnixEpoch());
  keyword.set_last_modified(base::Time::UnixEpoch());
  keyword.set_sync_guid("1234-5678-90AB-CDEF");
  keyword.SetSuggestionsURL("url2");
  keyword.SetURL("http://url/");
  keyword.set_favicon_url(GURL("http://favicon.url/"));
  ASSERT_TRUE(keyword_table->AddKeyword(keyword));

  keyword.set_originating_url(GURL("http://originating.url/"));
  keyword.set_autogenerate_keyword(true);
  EXPECT_EQ(ASCIIToUTF16("url"), keyword.keyword());
  keyword.add_input_encoding("Shift_JIS");
  SetID(1, &keyword);
  keyword.SetPrepopulateId(5);
  keyword.set_sync_guid("FEDC-BA09-8765-4321");
  keyword.SetInstantURL("http://instant2/");
  ASSERT_TRUE(keyword_table->AddKeyword(keyword));

  const char kTestContents[] = "1short_nameurlhttp://favicon.url/http://url/1"
      "http://originating.url/00Shift_JIS1url25100http://instant2/0"
      "FEDC-BA09-8765-43212short_namekeywordhttp://favicon.url/http://url/1001"
      "url2000001234-5678-90AB-CDEF";

  std::string contents;
  ASSERT_TRUE(keyword_table->GetTableContents("keywords", &contents));
  EXPECT_EQ(kTestContents, contents);

  ASSERT_TRUE(keyword_table->GetTableContents("keywords_backup", &contents));
  EXPECT_EQ(kTestContents, contents);
}

TEST_F(KeywordTableTest, UpdateKeyword) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));
  KeywordTable* keyword_table = db.GetKeywordTable();

  TemplateURL keyword;
  keyword.set_short_name(ASCIIToUTF16("short_name"));
  keyword.set_keyword(ASCIIToUTF16("keyword"));
  keyword.set_show_in_default_list(true);
  keyword.set_safe_for_autoreplace(true);
  SetID(1, &keyword);
  keyword.SetSuggestionsURL("url2");
  keyword.SetURL("http://url/");
  keyword.set_favicon_url(GURL("http://favicon.url/"));
  EXPECT_TRUE(keyword_table->AddKeyword(keyword));

  keyword.set_originating_url(GURL("http://originating.url/"));
  keyword.set_autogenerate_keyword(true);
  EXPECT_EQ(ASCIIToUTF16("url"), keyword.keyword());
  keyword.add_input_encoding("Shift_JIS");
  keyword.SetPrepopulateId(5);
  keyword.SetInstantURL("http://instant2/");
  EXPECT_TRUE(keyword_table->UpdateKeyword(keyword));

  KeywordTable::Keywords keywords;
  EXPECT_TRUE(keyword_table->GetKeywords(&keywords));
  EXPECT_EQ(1U, keywords.size());
  const TemplateURL* restored_keyword = keywords.front();

  EXPECT_EQ(keyword.short_name(), restored_keyword->short_name());
  EXPECT_EQ(keyword.originating_url(), restored_keyword->originating_url());
  EXPECT_EQ(keyword.keyword(), restored_keyword->keyword());
  EXPECT_EQ(keyword.autogenerate_keyword(),
            restored_keyword->autogenerate_keyword());
  EXPECT_EQ(keyword.show_in_default_list(),
            restored_keyword->show_in_default_list());
  EXPECT_EQ(keyword.safe_for_autoreplace(),
            restored_keyword->safe_for_autoreplace());
  EXPECT_EQ(keyword.input_encodings(), restored_keyword->input_encodings());
  EXPECT_EQ(keyword.id(), restored_keyword->id());
  EXPECT_EQ(keyword.prepopulate_id(), restored_keyword->prepopulate_id());
  ASSERT_TRUE(restored_keyword->suggestions_url());
  EXPECT_EQ(keyword.suggestions_url()->url(),
            restored_keyword->suggestions_url()->url());
  ASSERT_TRUE(restored_keyword->instant_url());
  EXPECT_EQ(keyword.favicon_url(), restored_keyword->favicon_url());
  EXPECT_EQ(keyword.instant_url()->url(),
            restored_keyword->instant_url()->url());

  STLDeleteElements(&keywords);
}

TEST_F(KeywordTableTest, KeywordWithNoFavicon) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));
  KeywordTable* keyword_table = db.GetKeywordTable();

  TemplateURL keyword;
  keyword.set_short_name(ASCIIToUTF16("short_name"));
  keyword.set_keyword(ASCIIToUTF16("keyword"));
  keyword.set_safe_for_autoreplace(true);
  SetID(-100, &keyword);
  keyword.SetURL("http://url/");
  EXPECT_TRUE(keyword_table->AddKeyword(keyword));

  KeywordTable::Keywords keywords;
  EXPECT_TRUE(keyword_table->GetKeywords(&keywords));
  EXPECT_EQ(1U, keywords.size());
  const TemplateURL* restored_keyword = keywords.front();

  EXPECT_EQ(keyword.short_name(), restored_keyword->short_name());
  EXPECT_EQ(keyword.keyword(), restored_keyword->keyword());
  EXPECT_EQ(keyword.safe_for_autoreplace(),
            restored_keyword->safe_for_autoreplace());
  EXPECT_EQ(keyword.id(), restored_keyword->id());
  EXPECT_EQ(keyword.favicon_url(), restored_keyword->favicon_url());

  STLDeleteElements(&keywords);
}
