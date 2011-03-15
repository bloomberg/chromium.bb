// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "app/sql/statement.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/guid.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/password_form.h"

using base::Time;
using base::TimeDelta;
using webkit_glue::FormField;
using webkit_glue::PasswordForm;

// So we can compare AutofillKeys with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillKey& key) {
  return os << UTF16ToASCII(key.name()) << ", " << UTF16ToASCII(key.value());
}

// So we can compare AutofillChanges with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillChange& change) {
  switch (change.type()) {
    case AutofillChange::ADD: {
      os << "ADD";
      break;
    }
    case AutofillChange::UPDATE: {
      os << "UPDATE";
      break;
    }
    case AutofillChange::REMOVE: {
      os << "REMOVE";
      break;
    }
  }
  return os << " " << change.key();
}

namespace {

bool CompareAutofillEntries(const AutofillEntry& a, const AutofillEntry& b) {
  std::set<base::Time> timestamps1(a.timestamps().begin(),
                                    a.timestamps().end());
  std::set<base::Time> timestamps2(b.timestamps().begin(),
                                    b.timestamps().end());

  int compVal = a.key().name().compare(b.key().name());
  if (compVal != 0) {
    return compVal < 0;
  }

  compVal = a.key().value().compare(b.key().value());
  if (compVal != 0) {
    return compVal < 0;
  }

  if (timestamps1.size() != timestamps2.size()) {
    return timestamps1.size() < timestamps2.size();
  }

  std::set<base::Time>::iterator it;
  for (it = timestamps1.begin(); it != timestamps1.end(); it++) {
    timestamps2.erase(*it);
  }

  return !timestamps2.empty();
}

void AutofillProfile31FromStatement(const sql::Statement& s,
                                    AutofillProfile* profile,
                                    string16* label,
                                    int* unique_id,
                                    int64* date_modified) {
  DCHECK(profile);
  DCHECK(label);
  DCHECK(unique_id);
  DCHECK(date_modified);
  *label = s.ColumnString16(0);
  *unique_id = s.ColumnInt(1);
  profile->SetInfo(AutofillType(NAME_FIRST), s.ColumnString16(2));
  profile->SetInfo(AutofillType(NAME_MIDDLE), s.ColumnString16(3));
  profile->SetInfo(AutofillType(NAME_LAST),s.ColumnString16(4));
  profile->SetInfo(AutofillType(EMAIL_ADDRESS), s.ColumnString16(5));
  profile->SetInfo(AutofillType(COMPANY_NAME), s.ColumnString16(6));
  profile->SetInfo(AutofillType(ADDRESS_HOME_LINE1), s.ColumnString16(7));
  profile->SetInfo(AutofillType(ADDRESS_HOME_LINE2), s.ColumnString16(8));
  profile->SetInfo(AutofillType(ADDRESS_HOME_CITY), s.ColumnString16(9));
  profile->SetInfo(AutofillType(ADDRESS_HOME_STATE), s.ColumnString16(10));
  profile->SetInfo(AutofillType(ADDRESS_HOME_ZIP), s.ColumnString16(11));
  profile->SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), s.ColumnString16(12));
  profile->SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), s.ColumnString16(13));
  profile->SetInfo(AutofillType(PHONE_FAX_WHOLE_NUMBER), s.ColumnString16(14));
  *date_modified = s.ColumnInt64(15);
  profile->set_guid(s.ColumnString(16));
  EXPECT_TRUE(guid::IsValidGUID(profile->guid()));
}

void AutofillProfile32FromStatement(const sql::Statement& s,
                                    AutofillProfile* profile,
                                    string16* label,
                                    int64* date_modified) {
  DCHECK(profile);
  DCHECK(label);
  DCHECK(date_modified);
  profile->set_guid(s.ColumnString(0));
  EXPECT_TRUE(guid::IsValidGUID(profile->guid()));
  *label = s.ColumnString16(1);
  profile->SetInfo(AutofillType(NAME_FIRST), s.ColumnString16(2));
  profile->SetInfo(AutofillType(NAME_MIDDLE), s.ColumnString16(3));
  profile->SetInfo(AutofillType(NAME_LAST),s.ColumnString16(4));
  profile->SetInfo(AutofillType(EMAIL_ADDRESS), s.ColumnString16(5));
  profile->SetInfo(AutofillType(COMPANY_NAME), s.ColumnString16(6));
  profile->SetInfo(AutofillType(ADDRESS_HOME_LINE1), s.ColumnString16(7));
  profile->SetInfo(AutofillType(ADDRESS_HOME_LINE2), s.ColumnString16(8));
  profile->SetInfo(AutofillType(ADDRESS_HOME_CITY), s.ColumnString16(9));
  profile->SetInfo(AutofillType(ADDRESS_HOME_STATE), s.ColumnString16(10));
  profile->SetInfo(AutofillType(ADDRESS_HOME_ZIP), s.ColumnString16(11));
  profile->SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), s.ColumnString16(12));
  profile->SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), s.ColumnString16(13));
  profile->SetInfo(AutofillType(PHONE_FAX_WHOLE_NUMBER), s.ColumnString16(14));
  *date_modified = s.ColumnInt64(15);
}

void AutofillProfile33FromStatement(const sql::Statement& s,
                                    AutofillProfile* profile,
                                    int64* date_modified) {
  DCHECK(profile);
  DCHECK(date_modified);
  profile->set_guid(s.ColumnString(0));
  EXPECT_TRUE(guid::IsValidGUID(profile->guid()));
  profile->SetInfo(AutofillType(COMPANY_NAME), s.ColumnString16(1));
  profile->SetInfo(AutofillType(ADDRESS_HOME_LINE1), s.ColumnString16(2));
  profile->SetInfo(AutofillType(ADDRESS_HOME_LINE2), s.ColumnString16(3));
  profile->SetInfo(AutofillType(ADDRESS_HOME_CITY), s.ColumnString16(4));
  profile->SetInfo(AutofillType(ADDRESS_HOME_STATE), s.ColumnString16(5));
  profile->SetInfo(AutofillType(ADDRESS_HOME_ZIP), s.ColumnString16(6));
  profile->SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), s.ColumnString16(7));
  *date_modified = s.ColumnInt64(8);
}

void CreditCard31FromStatement(const sql::Statement& s,
                              CreditCard* credit_card,
                              string16* label,
                              int* unique_id,
                              std::string* encrypted_number,
                              int64* date_modified) {
  DCHECK(credit_card);
  DCHECK(label);
  DCHECK(unique_id);
  DCHECK(encrypted_number);
  DCHECK(date_modified);
  *label = s.ColumnString16(0);
  *unique_id = s.ColumnInt(1);
  credit_card->SetInfo(AutofillType(CREDIT_CARD_NAME), s.ColumnString16(2));
  credit_card->SetInfo(AutofillType(CREDIT_CARD_TYPE), s.ColumnString16(3));
  credit_card->SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH),
                       s.ColumnString16(5));
  credit_card->SetInfo(AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                       s.ColumnString16(6));
  int encrypted_number_len = s.ColumnByteLength(10);
  if (encrypted_number_len) {
    encrypted_number->resize(encrypted_number_len);
    memcpy(&(*encrypted_number)[0], s.ColumnBlob(10), encrypted_number_len);
  }
  *date_modified = s.ColumnInt64(12);
  credit_card->set_guid(s.ColumnString(13));
  EXPECT_TRUE(guid::IsValidGUID(credit_card->guid()));
}

void CreditCard32FromStatement(const sql::Statement& s,
                               CreditCard* credit_card,
                               std::string* encrypted_number,
                               int64* date_modified) {
  DCHECK(credit_card);
  DCHECK(encrypted_number);
  DCHECK(date_modified);
  credit_card->set_guid(s.ColumnString(0));
  EXPECT_TRUE(guid::IsValidGUID(credit_card->guid()));
  credit_card->SetInfo(AutofillType(CREDIT_CARD_NAME), s.ColumnString16(1));
  credit_card->SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH),
                       s.ColumnString16(2));
  credit_card->SetInfo(AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                       s.ColumnString16(3));
  int encrypted_number_len = s.ColumnByteLength(4);
  if (encrypted_number_len) {
    encrypted_number->resize(encrypted_number_len);
    memcpy(&(*encrypted_number)[0], s.ColumnBlob(4), encrypted_number_len);
  }
  *date_modified = s.ColumnInt64(5);
}

}  // namespace

class WebDatabaseTest : public testing::Test {
 public:
  WebDatabaseTest() {}
  virtual ~WebDatabaseTest() {}

 protected:
  typedef std::vector<AutofillChange> AutofillChangeList;
  typedef std::set<AutofillEntry,
    bool (*)(const AutofillEntry&, const AutofillEntry&)> AutofillEntrySet;
  typedef std::set<AutofillEntry, bool (*)(const AutofillEntry&,
    const AutofillEntry&)>::iterator AutofillEntrySetIterator;
  virtual void SetUp() {
#if defined(OS_MACOSX)
    Encryptor::UseMockKeychain(true);
#endif
    PathService::Get(chrome::DIR_TEST_DATA, &file_);
    const std::string test_db = "TestWebDatabase" +
        base::Int64ToString(base::Time::Now().ToTimeT()) +
        ".db";
    file_ = file_.AppendASCII(test_db);
    file_util::Delete(file_, false);
  }

  virtual void TearDown() {
    file_util::Delete(file_, false);
  }

  static int GetKeyCount(const DictionaryValue& d) {
    DictionaryValue::key_iterator i(d.begin_keys());
    DictionaryValue::key_iterator e(d.end_keys());

    int r = 0;
    while (i != e) {
      ++i;
      ++r;
    }
    return r;
  }

  static bool StringDictionaryValueEquals(const DictionaryValue& a,
                                          const DictionaryValue& b) {
    int a_count = 0;
    int b_count = GetKeyCount(b);
    DictionaryValue::key_iterator i(a.begin_keys());
    DictionaryValue::key_iterator e(a.end_keys());
    std::string av, bv;
    while (i != e) {
      if (!(a.GetStringWithoutPathExpansion(*i, &av)) ||
          !(b.GetStringWithoutPathExpansion(*i, &bv)) ||
          av != bv)
        return false;

      a_count++;
      ++i;
    }

    return (a_count == b_count);
  }

  static int64 GetID(const TemplateURL* url) {
    return url->id();
  }

  static void SetID(int64 new_id, TemplateURL* url) {
    url->set_id(new_id);
  }

  static void set_prepopulate_id(TemplateURL* url, int id) {
    url->set_prepopulate_id(id);
  }

  static void set_logo_id(TemplateURL* url, int id) {
    url->set_logo_id(id);
  }

  static AutofillEntry MakeAutofillEntry(const char* name,
                                         const char* value,
                                         time_t timestamp0,
                                         time_t timestamp1) {
    std::vector<base::Time> timestamps;
    if (timestamp0 >= 0)
      timestamps.push_back(Time::FromTimeT(timestamp0));
    if (timestamp1 >= 0)
      timestamps.push_back(Time::FromTimeT(timestamp1));
    return AutofillEntry(
        AutofillKey(ASCIIToUTF16(name), ASCIIToUTF16(value)), timestamps);
  }

  FilePath file_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDatabaseTest);
};

TEST_F(WebDatabaseTest, Keywords) {
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
  base::Time created_time = Time::Now();
  template_url.set_date_created(created_time);
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

  EXPECT_TRUE(db.AddKeyword(template_url));

  std::vector<TemplateURL*> template_urls;
  EXPECT_TRUE(db.GetKeywords(&template_urls));

  EXPECT_EQ(1U, template_urls.size());
  const TemplateURL* restored_url = template_urls.front();

  EXPECT_EQ(template_url.short_name(), restored_url->short_name());

  EXPECT_EQ(template_url.keyword(), restored_url->keyword());

  EXPECT_FALSE(restored_url->autogenerate_keyword());

  EXPECT_TRUE(favicon_url == restored_url->GetFaviconURL());

  EXPECT_TRUE(restored_url->safe_for_autoreplace());

  // The database stores time only at the resolution of a second.
  EXPECT_EQ(created_time.ToTimeT(), restored_url->date_created().ToTimeT());

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

  EXPECT_TRUE(db.RemoveKeyword(restored_url->id()));

  template_urls.clear();
  EXPECT_TRUE(db.GetKeywords(&template_urls));

  EXPECT_EQ(0U, template_urls.size());

  delete restored_url;
}

TEST_F(WebDatabaseTest, KeywordMisc) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  ASSERT_EQ(0, db.GetDefaulSearchProviderID());
  ASSERT_EQ(0, db.GetBuitinKeywordVersion());

  db.SetDefaultSearchProviderID(10);
  db.SetBuitinKeywordVersion(11);

  ASSERT_EQ(10, db.GetDefaulSearchProviderID());
  ASSERT_EQ(11, db.GetBuitinKeywordVersion());
}

TEST_F(WebDatabaseTest, UpdateKeyword) {
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

  EXPECT_TRUE(db.AddKeyword(template_url));

  GURL originating_url2("http://originating.url/");
  template_url.set_originating_url(originating_url2);
  template_url.set_autogenerate_keyword(true);
  EXPECT_EQ(ASCIIToUTF16("url"), template_url.keyword());
  template_url.add_input_encoding("Shift_JIS");
  set_prepopulate_id(&template_url, 5);
  set_logo_id(&template_url, 2000);
  template_url.SetInstantURL("http://instant2/", 0, 0);
  EXPECT_TRUE(db.UpdateKeyword(template_url));

  std::vector<TemplateURL*> template_urls;
  EXPECT_TRUE(db.GetKeywords(&template_urls));

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

TEST_F(WebDatabaseTest, KeywordWithNoFavicon) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  TemplateURL template_url;
  template_url.set_short_name(ASCIIToUTF16("short_name"));
  template_url.set_keyword(ASCIIToUTF16("keyword"));
  template_url.SetURL("http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  SetID(-100, &template_url);

  EXPECT_TRUE(db.AddKeyword(template_url));

  std::vector<TemplateURL*> template_urls;
  EXPECT_TRUE(db.GetKeywords(&template_urls));
  EXPECT_EQ(1U, template_urls.size());
  const TemplateURL* restored_url = template_urls.front();

  EXPECT_EQ(template_url.short_name(), restored_url->short_name());
  EXPECT_EQ(template_url.keyword(), restored_url->keyword());
  EXPECT_TRUE(!restored_url->GetFaviconURL().is_valid());
  EXPECT_TRUE(restored_url->safe_for_autoreplace());
  EXPECT_EQ(GetID(&template_url), GetID(restored_url));
  delete restored_url;
}

TEST_F(WebDatabaseTest, Logins) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("http://www.google.com/accounts/LoginAuth");
  form.action = GURL("http://www.google.com/accounts/Login");
  form.username_element = ASCIIToUTF16("Email");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("Passwd");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = "http://www.google.com/";
  form.ssl_valid = false;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_TRUE(db.AddLogin(form));
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db.GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The example site changes...
  PasswordForm form2(form);
  form2.origin = GURL("http://www.google.com/new/accounts/LoginAuth");
  form2.submit_element = ASCIIToUTF16("reallySignIn");

  // Match against an inexact copy
  EXPECT_TRUE(db.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Uh oh, the site changed origin & action URL's all at once!
  PasswordForm form3(form2);
  form3.action = GURL("http://www.google.com/new/accounts/Login");

  // signon_realm is the same, should match.
  EXPECT_TRUE(db.GetLogins(form3, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Imagine the site moves to a secure server for login.
  PasswordForm form4(form3);
  form4.signon_realm = "https://www.google.com/";
  form4.ssl_valid = true;

  // We have only an http record, so no match for this.
  EXPECT_TRUE(db.GetLogins(form4, &result));
  EXPECT_EQ(0U, result.size());

  // Let's imagine the user logs into the secure site.
  EXPECT_TRUE(db.AddLogin(form4));
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(2U, result.size());
  delete result[0];
  delete result[1];
  result.clear();

  // Now the match works
  EXPECT_TRUE(db.GetLogins(form4, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The user chose to forget the original but not the new.
  EXPECT_TRUE(db.RemoveLogin(form));
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The old form wont match the new site (http vs https).
  EXPECT_TRUE(db.GetLogins(form, &result));
  EXPECT_EQ(0U, result.size());

  // The user's request for the HTTPS site is intercepted
  // by an attacker who presents an invalid SSL cert.
  PasswordForm form5(form4);
  form5.ssl_valid = 0;

  // It will match in this case.
  EXPECT_TRUE(db.GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // User changes his password.
  PasswordForm form6(form5);
  form6.password_value = ASCIIToUTF16("test6");
  form6.preferred = true;

  // We update, and check to make sure it matches the
  // old form, and there is only one record.
  EXPECT_TRUE(db.UpdateLogin(form6));
  // matches
  EXPECT_TRUE(db.GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();
  // Only one record.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  // password element was updated.
  EXPECT_EQ(form6.password_value, result[0]->password_value);
  // Preferred login.
  EXPECT_TRUE(form6.preferred);
  delete result[0];
  result.clear();

  // Make sure everything can disappear.
  EXPECT_TRUE(db.RemoveLogin(form4));
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());
}

TEST_F(WebDatabaseTest, Autofill) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  Time t1 = Time::Now();

  // Simulate the submission of a handful of entries in a field called "Name",
  // some more often than others.
  AutofillChangeList changes;
  EXPECT_TRUE(db.AddFormFieldValue(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Superman"),
                string16(),
                0,
                false),
      &changes));
  std::vector<string16> v;
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(db.AddFormFieldValue(
        FormField(string16(),
                  ASCIIToUTF16("Name"),
                  ASCIIToUTF16("Clark Kent"),
                  string16(),
                  0,
                  false),
        &changes));
  }
  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(db.AddFormFieldValue(
        FormField(string16(),
                  ASCIIToUTF16("Name"),
                  ASCIIToUTF16("Clark Sutter"),
                  string16(),
                  0,
                  false),
        &changes));
  }
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(db.AddFormFieldValue(
        FormField(string16(),
                  ASCIIToUTF16("Favorite Color"),
                  ASCIIToUTF16("Green"),
                  string16(),
                  0,
                  false),
        &changes));
  }

  int count = 0;
  int64 pair_id = 0;

  // We have added the name Clark Kent 5 times, so count should be 5 and pair_id
  // should be somthing non-zero.
  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Clark Kent"),
                string16(),
                0,
                false),
      &pair_id, &count));
  EXPECT_EQ(5, count);
  EXPECT_NE(0, pair_id);

  // Storing in the data base should be case sensitive, so there should be no
  // database entry for clark kent lowercase.
  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("clark kent"),
                string16(),
                0,
                false),
      &pair_id, &count));
  EXPECT_EQ(0, count);

  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      FormField(string16(),
                ASCIIToUTF16("Favorite Color"),
                ASCIIToUTF16("Green"),
                string16(),
                0,
                false),
      &pair_id, &count));
  EXPECT_EQ(2, count);

  // This is meant to get a list of suggestions for Name.  The empty prefix
  // in the second argument means it should return all suggestions for a name
  // no matter what they start with.  The order that the names occur in the list
  // should be decreasing order by count.
  EXPECT_TRUE(db.GetFormValuesForElementName(
      ASCIIToUTF16("Name"), string16(), &v, 6));
  EXPECT_EQ(3U, v.size());
  if (v.size() == 3) {
    EXPECT_EQ(ASCIIToUTF16("Clark Kent"), v[0]);
    EXPECT_EQ(ASCIIToUTF16("Clark Sutter"), v[1]);
    EXPECT_EQ(ASCIIToUTF16("Superman"), v[2]);
  }

  // If we query again limiting the list size to 1, we should only get the most
  // frequent entry.
  EXPECT_TRUE(db.GetFormValuesForElementName(
      ASCIIToUTF16("Name"), string16(), &v, 1));
  EXPECT_EQ(1U, v.size());
  if (v.size() == 1) {
    EXPECT_EQ(ASCIIToUTF16("Clark Kent"), v[0]);
  }

  // Querying for suggestions given a prefix is case-insensitive, so the prefix
  // "cLa" shoud get suggestions for both Clarks.
  EXPECT_TRUE(db.GetFormValuesForElementName(
      ASCIIToUTF16("Name"), ASCIIToUTF16("cLa"), &v, 6));
  EXPECT_EQ(2U, v.size());
  if (v.size() == 2) {
    EXPECT_EQ(ASCIIToUTF16("Clark Kent"), v[0]);
    EXPECT_EQ(ASCIIToUTF16("Clark Sutter"), v[1]);
  }

  // Removing all elements since the beginning of this function should remove
  // everything from the database.
  changes.clear();
  EXPECT_TRUE(db.RemoveFormElementsAddedBetween(t1, Time(), &changes));

  const AutofillChange expected_changes[] = {
    AutofillChange(AutofillChange::REMOVE,
                   AutofillKey(ASCIIToUTF16("Name"),
                               ASCIIToUTF16("Superman"))),
    AutofillChange(AutofillChange::REMOVE,
                   AutofillKey(ASCIIToUTF16("Name"),
                               ASCIIToUTF16("Clark Kent"))),
    AutofillChange(AutofillChange::REMOVE,
                   AutofillKey(ASCIIToUTF16("Name"),
                               ASCIIToUTF16("Clark Sutter"))),
    AutofillChange(AutofillChange::REMOVE,
                   AutofillKey(ASCIIToUTF16("Favorite Color"),
                               ASCIIToUTF16("Green"))),
  };
  EXPECT_EQ(arraysize(expected_changes), changes.size());
  for (size_t i = 0; i < arraysize(expected_changes); i++) {
    EXPECT_EQ(expected_changes[i], changes[i]);
  }

  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Clark Kent"),
                string16(),
                0,
                false),
      &pair_id, &count));
  EXPECT_EQ(0, count);

  EXPECT_TRUE(
      db.GetFormValuesForElementName(ASCIIToUTF16("Name"), string16(), &v, 6));
  EXPECT_EQ(0U, v.size());

  // Now add some values with empty strings.
  const string16 kValue = ASCIIToUTF16("  toto   ");
  EXPECT_TRUE(db.AddFormFieldValue(FormField(string16(),
                                             ASCIIToUTF16("blank"),
                                             string16(),
                                             string16(),
                                             0,
                                             false),
                                   &changes));
  EXPECT_TRUE(db.AddFormFieldValue(FormField(string16(),
                                             ASCIIToUTF16("blank"),
                                             ASCIIToUTF16(" "),
                                             string16(),
                                             0,
                                             false),
                                   &changes));
  EXPECT_TRUE(db.AddFormFieldValue(FormField(string16(),
                                             ASCIIToUTF16("blank"),
                                             ASCIIToUTF16("      "),
                                             string16(),
                                             0,
                                             false),
                                   &changes));
  EXPECT_TRUE(db.AddFormFieldValue(FormField(string16(),
                                             ASCIIToUTF16("blank"),
                                             kValue,
                                             string16(),
                                             0,
                                             false),
                                   &changes));

  // They should be stored normally as the DB layer does not check for empty
  // values.
  v.clear();
  EXPECT_TRUE(db.GetFormValuesForElementName(
      ASCIIToUTF16("blank"), string16(), &v, 10));
  EXPECT_EQ(4U, v.size());

  // Now we'll check that ClearAutofillEmptyValueElements() works as expected.
  db.ClearAutofillEmptyValueElements();

  v.clear();
  EXPECT_TRUE(db.GetFormValuesForElementName(ASCIIToUTF16("blank"),
      string16(), &v, 10));
  ASSERT_EQ(1U, v.size());

  EXPECT_EQ(kValue, v[0]);
}

TEST_F(WebDatabaseTest, Autofill_RemoveBetweenChanges) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  TimeDelta one_day(TimeDelta::FromDays(1));
  Time t1 = Time::Now();
  Time t2 = t1 + one_day;

  AutofillChangeList changes;
  EXPECT_TRUE(db.AddFormFieldValueTime(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Superman"),
                string16(),
                0,
                false),
      &changes,
      t1));
  EXPECT_TRUE(db.AddFormFieldValueTime(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Superman"),
                string16(),
                0,
                false),
      &changes,
      t2));

  changes.clear();
  EXPECT_TRUE(db.RemoveFormElementsAddedBetween(t1, t2, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::UPDATE,
                             AutofillKey(ASCIIToUTF16("Name"),
                                         ASCIIToUTF16("Superman"))),
            changes[0]);
  changes.clear();

  EXPECT_TRUE(db.RemoveFormElementsAddedBetween(t2, t2 + one_day, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::REMOVE,
                             AutofillKey(ASCIIToUTF16("Name"),
                                         ASCIIToUTF16("Superman"))),
            changes[0]);
}

TEST_F(WebDatabaseTest, Autofill_AddChanges) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  TimeDelta one_day(TimeDelta::FromDays(1));
  Time t1 = Time::Now();
  Time t2 = t1 + one_day;

  AutofillChangeList changes;
  EXPECT_TRUE(db.AddFormFieldValueTime(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Superman"),
                string16(),
                0,
                false),
      &changes,
      t1));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::ADD,
                             AutofillKey(ASCIIToUTF16("Name"),
                                         ASCIIToUTF16("Superman"))),
            changes[0]);

  changes.clear();
  EXPECT_TRUE(db.AddFormFieldValueTime(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Superman"),
                string16(),
                0,
                false),
      &changes,
      t2));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::UPDATE,
                             AutofillKey(ASCIIToUTF16("Name"),
                                         ASCIIToUTF16("Superman"))),
            changes[0]);
}

TEST_F(WebDatabaseTest, Autofill_UpdateOneWithOneTimestamp) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillEntry entry(MakeAutofillEntry("foo", "bar", 1, -1));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(db.UpdateAutofillEntries(entries));

  FormField field(string16(),
                  ASCIIToUTF16("foo"),
                  ASCIIToUTF16("bar"),
                  string16(),
                  0,
                  false);
  int64 pair_id;
  int count;
  ASSERT_TRUE(db.GetIDAndCountOfFormElement(field, &pair_id, &count));
  EXPECT_LE(0, pair_id);
  EXPECT_EQ(1, count);

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(db.GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_TRUE(entry == all_entries[0]);
}

TEST_F(WebDatabaseTest, Autofill_UpdateOneWithTwoTimestamps) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillEntry entry(MakeAutofillEntry("foo", "bar", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(db.UpdateAutofillEntries(entries));

  FormField field(string16(),
                  ASCIIToUTF16("foo"),
                  ASCIIToUTF16("bar"),
                  string16(),
                  0,
                  false);
  int64 pair_id;
  int count;
  ASSERT_TRUE(db.GetIDAndCountOfFormElement(field, &pair_id, &count));
  EXPECT_LE(0, pair_id);
  EXPECT_EQ(2, count);

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(db.GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_TRUE(entry == all_entries[0]);
}

TEST_F(WebDatabaseTest, Autofill_GetAutofillTimestamps) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillEntry entry(MakeAutofillEntry("foo", "bar", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(db.UpdateAutofillEntries(entries));

  std::vector<base::Time> timestamps;
  ASSERT_TRUE(db.GetAutofillTimestamps(ASCIIToUTF16("foo"),
                                       ASCIIToUTF16("bar"),
                                       &timestamps));
  ASSERT_EQ(2U, timestamps.size());
  EXPECT_TRUE(Time::FromTimeT(1) == timestamps[0]);
  EXPECT_TRUE(Time::FromTimeT(2) == timestamps[1]);
}

TEST_F(WebDatabaseTest, Autofill_UpdateTwo) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillEntry entry0(MakeAutofillEntry("foo", "bar0", 1, -1));
  AutofillEntry entry1(MakeAutofillEntry("foo", "bar1", 2, 3));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry0);
  entries.push_back(entry1);
  ASSERT_TRUE(db.UpdateAutofillEntries(entries));

  FormField field0(string16(),
                   ASCIIToUTF16("foo"),
                   ASCIIToUTF16("bar0"),
                   string16(),
                   0,
                   false);
  int64 pair_id;
  int count;
  ASSERT_TRUE(db.GetIDAndCountOfFormElement(field0, &pair_id, &count));
  EXPECT_LE(0, pair_id);
  EXPECT_EQ(1, count);

  FormField field1(string16(),
                   ASCIIToUTF16("foo"),
                   ASCIIToUTF16("bar1"),
                   string16(),
                   0,
                   false);
  ASSERT_TRUE(db.GetIDAndCountOfFormElement(field1, &pair_id, &count));
  EXPECT_LE(0, pair_id);
  EXPECT_EQ(2, count);
}

TEST_F(WebDatabaseTest, Autofill_UpdateReplace) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillChangeList changes;
  // Add a form field.  This will be replaced.
  EXPECT_TRUE(db.AddFormFieldValue(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Superman"),
                string16(),
                0,
                false),
      &changes));

  AutofillEntry entry(MakeAutofillEntry("Name", "Superman", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(db.UpdateAutofillEntries(entries));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(db.GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_TRUE(entry == all_entries[0]);
}

TEST_F(WebDatabaseTest, Autofill_UpdateDontReplace) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  Time t = Time::Now();
  AutofillEntry existing(
      MakeAutofillEntry("Name", "Superman", t.ToTimeT(), -1));

  AutofillChangeList changes;
  // Add a form field.  This will NOT be replaced.
  EXPECT_TRUE(db.AddFormFieldValueTime(
      FormField(string16(),
                existing.key().name(),
                existing.key().value(),
                string16(),
                0,
                false),
      &changes,
      t));
  AutofillEntry entry(MakeAutofillEntry("Name", "Clark Kent", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(db.UpdateAutofillEntries(entries));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(db.GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
  AutofillEntrySet expected_entries(all_entries.begin(),
                                    all_entries.end(),
                                    CompareAutofillEntries);
  EXPECT_EQ(1U, expected_entries.count(existing));
  EXPECT_EQ(1U, expected_entries.count(entry));
}

TEST_F(WebDatabaseTest, Autofill_AddFormFieldValues) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  Time t = Time::Now();

  // Add multiple values for "firstname" and "lastname" names.  Test that only
  // first value of each gets added. Related to security issue:
  // http://crbug.com/51727.
  std::vector<FormField> elements;
  elements.push_back(FormField(string16(),
                               ASCIIToUTF16("firstname"),
                               ASCIIToUTF16("Joe"),
                               string16(),
                               0,
                               false));
  elements.push_back(FormField(string16(),
                               ASCIIToUTF16("firstname"),
                               ASCIIToUTF16("Jane"),
                               string16(),
                               0,
                               false));
  elements.push_back(FormField(string16(),
                               ASCIIToUTF16("lastname"),
                               ASCIIToUTF16("Smith"),
                               string16(),
                               0,
                               false));
  elements.push_back(FormField(string16(),
                               ASCIIToUTF16("lastname"),
                               ASCIIToUTF16("Jones"),
                               string16(),
                               0,
                               false));

  std::vector<AutofillChange> changes;
  db.AddFormFieldValuesTime(elements, &changes, t);

  ASSERT_EQ(2U, changes.size());
  EXPECT_EQ(changes[0], AutofillChange(AutofillChange::ADD,
                                       AutofillKey(ASCIIToUTF16("firstname"),
                                       ASCIIToUTF16("Joe"))));
  EXPECT_EQ(changes[1], AutofillChange(AutofillChange::ADD,
                                       AutofillKey(ASCIIToUTF16("lastname"),
                                       ASCIIToUTF16("Smith"))));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(db.GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
}

static bool AddTimestampedLogin(WebDatabase* db, std::string url,
                                const std::string& unique_string,
                                const Time& time) {
  // Example password form.
  PasswordForm form;
  form.origin = GURL(url + std::string("/LoginAuth"));
  form.username_element = ASCIIToUTF16(unique_string);
  form.username_value = ASCIIToUTF16(unique_string);
  form.password_element = ASCIIToUTF16(unique_string);
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = url;
  form.date_created = time;
  return db->AddLogin(form);
}

static void ClearResults(std::vector<PasswordForm*>* results) {
  for (size_t i = 0; i < results->size(); ++i) {
    delete (*results)[i];
  }
  results->clear();
}

TEST_F(WebDatabaseTest, ClearPrivateData_SavedPasswords) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());

  Time now = Time::Now();
  TimeDelta one_day = TimeDelta::FromDays(1);

  // Create one with a 0 time.
  EXPECT_TRUE(AddTimestampedLogin(&db, "1", "foo1", Time()));
  // Create one for now and +/- 1 day.
  EXPECT_TRUE(AddTimestampedLogin(&db, "2", "foo2", now - one_day));
  EXPECT_TRUE(AddTimestampedLogin(&db, "3", "foo3", now));
  EXPECT_TRUE(AddTimestampedLogin(&db, "4", "foo4", now + one_day));

  // Verify inserts worked.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(4U, result.size());
  ClearResults(&result);

  // Delete everything from today's date and on.
  db.RemoveLoginsCreatedBetween(now, Time());

  // Should have deleted half of what we inserted.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(2U, result.size());
  ClearResults(&result);

  // Delete with 0 date (should delete all).
  db.RemoveLoginsCreatedBetween(Time(), Time());

  // Verify nothing is left.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());
}

TEST_F(WebDatabaseTest, BlacklistedLogins) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));
  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  ASSERT_EQ(0U, result.size());

  // Save a form as blacklisted.
  PasswordForm form;
  form.origin = GURL("http://www.google.com/accounts/LoginAuth");
  form.action = GURL("http://www.google.com/accounts/Login");
  form.username_element = ASCIIToUTF16("Email");
  form.password_element = ASCIIToUTF16("Passwd");
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = "http://www.google.com/";
  form.ssl_valid = false;
  form.preferred = true;
  form.blacklisted_by_user = true;
  form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_TRUE(db.AddLogin(form));

  // Get all non-blacklisted logins (should be none).
  EXPECT_TRUE(db.GetAllLogins(&result, false));
  ASSERT_EQ(0U, result.size());

  // GetLogins should give the blacklisted result.
  EXPECT_TRUE(db.GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  ClearResults(&result);

  // So should GetAll including blacklisted.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  ClearResults(&result);
}

TEST_F(WebDatabaseTest, WebAppHasAllImages) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));
  GURL url("http://google.com/");

  // Initial value for unknown web app should be false.
  EXPECT_FALSE(db.GetWebAppHasAllImages(url));

  // Set the value and make sure it took.
  EXPECT_TRUE(db.SetWebAppHasAllImages(url, true));
  EXPECT_TRUE(db.GetWebAppHasAllImages(url));

  // Remove the app and make sure value reverts to default.
  EXPECT_TRUE(db.RemoveWebApp(url));
  EXPECT_FALSE(db.GetWebAppHasAllImages(url));
}

TEST_F(WebDatabaseTest, WebAppImages) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));
  GURL url("http://google.com/");

  // Web app should initially have no images.
  std::vector<SkBitmap> images;
  ASSERT_TRUE(db.GetWebAppImages(url, &images));
  ASSERT_EQ(0U, images.size());

  // Add an image.
  SkBitmap image;
  image.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  image.allocPixels();
  image.eraseColor(SK_ColorBLACK);
  ASSERT_TRUE(db.SetWebAppImage(url, image));

  // Make sure we get the image back.
  ASSERT_TRUE(db.GetWebAppImages(url, &images));
  ASSERT_EQ(1U, images.size());
  ASSERT_EQ(16, images[0].width());
  ASSERT_EQ(16, images[0].height());

  // Add another 16x16 image and make sure it replaces the original.
  image.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  image.allocPixels();
  image.eraseColor(SK_ColorBLACK);

  // Set some random pixels so that we can identify the image. We don't use
  // transparent images because of pre-multiplication rounding errors.
  SkColor test_pixel_1 = 0xffccbbaa;
  SkColor test_pixel_2 = 0xffaabbaa;
  SkColor test_pixel_3 = 0xff339966;
  image.getAddr32(0, 1)[0] = test_pixel_1;
  image.getAddr32(0, 1)[1] = test_pixel_2;
  image.getAddr32(0, 1)[2] = test_pixel_3;

  ASSERT_TRUE(db.SetWebAppImage(url, image));
  images.clear();
  ASSERT_TRUE(db.GetWebAppImages(url, &images));
  ASSERT_EQ(1U, images.size());
  ASSERT_EQ(16, images[0].width());
  ASSERT_EQ(16, images[0].height());
  images[0].lockPixels();
  ASSERT_TRUE(images[0].getPixels() != NULL);
  ASSERT_EQ(test_pixel_1, images[0].getAddr32(0, 1)[0]);
  ASSERT_EQ(test_pixel_2, images[0].getAddr32(0, 1)[1]);
  ASSERT_EQ(test_pixel_3, images[0].getAddr32(0, 1)[2]);
  images[0].unlockPixels();

  // Add another image at a bigger size.
  image.setConfig(SkBitmap::kARGB_8888_Config, 32, 32);
  image.allocPixels();
  image.eraseColor(SK_ColorBLACK);
  ASSERT_TRUE(db.SetWebAppImage(url, image));

  // Make sure we get both images back.
  images.clear();
  ASSERT_TRUE(db.GetWebAppImages(url, &images));
  ASSERT_EQ(2U, images.size());
  if (images[0].width() == 16) {
    ASSERT_EQ(16, images[0].width());
    ASSERT_EQ(16, images[0].height());
    ASSERT_EQ(32, images[1].width());
    ASSERT_EQ(32, images[1].height());
  } else {
    ASSERT_EQ(32, images[0].width());
    ASSERT_EQ(32, images[0].height());
    ASSERT_EQ(16, images[1].width());
    ASSERT_EQ(16, images[1].height());
  }
}

TEST_F(WebDatabaseTest, TokenServiceGetAllRemoveAll) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::map<std::string, std::string> out_map;
  std::string service;
  std::string service2;
  service = "testservice";
  service2 = "othertestservice";

  EXPECT_TRUE(db.GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  // Check that get all tokens works
  EXPECT_TRUE(db.SetTokenForService(service, "pepperoni"));
  EXPECT_TRUE(db.SetTokenForService(service2, "steak"));
  EXPECT_TRUE(db.GetAllTokens(&out_map));
  EXPECT_EQ(out_map.find(service)->second, "pepperoni");
  EXPECT_EQ(out_map.find(service2)->second, "steak");
  out_map.clear();

  // Purge
  EXPECT_TRUE(db.RemoveAllTokens());
  EXPECT_TRUE(db.GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  // Check that you can still add it back in
  EXPECT_TRUE(db.SetTokenForService(service, "cheese"));
  EXPECT_TRUE(db.GetAllTokens(&out_map));
  EXPECT_EQ(out_map.find(service)->second, "cheese");
}

TEST_F(WebDatabaseTest, TokenServiceGetSet) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::map<std::string, std::string> out_map;
  std::string service;
  service = "testservice";

  EXPECT_TRUE(db.GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  EXPECT_TRUE(db.SetTokenForService(service, "pepperoni"));
  EXPECT_TRUE(db.GetAllTokens(&out_map));
  EXPECT_EQ(out_map.find(service)->second, "pepperoni");
  out_map.clear();

  // try blanking it - won't remove it from the db though!
  EXPECT_TRUE(db.SetTokenForService(service, ""));
  EXPECT_TRUE(db.GetAllTokens(&out_map));
  EXPECT_EQ(out_map.find(service)->second, "");
  out_map.clear();

  // try mutating it
  EXPECT_TRUE(db.SetTokenForService(service, "ham"));
  EXPECT_TRUE(db.GetAllTokens(&out_map));
  EXPECT_EQ(out_map.find(service)->second, "ham");
}

TEST_F(WebDatabaseTest, AutofillProfile) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Add a 'Home' profile.
  AutofillProfile home_profile;
  home_profile.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("John"));
  home_profile.SetInfo(AutofillType(NAME_MIDDLE), ASCIIToUTF16("Q."));
  home_profile.SetInfo(AutofillType(NAME_LAST), ASCIIToUTF16("Smith"));
  home_profile.SetInfo(AutofillType(EMAIL_ADDRESS),
                       ASCIIToUTF16("js@smith.xyz"));
  home_profile.SetInfo(AutofillType(COMPANY_NAME), ASCIIToUTF16("Google"));
  home_profile.SetInfo(AutofillType(ADDRESS_HOME_LINE1),
                       ASCIIToUTF16("1234 Apple Way"));
  home_profile.SetInfo(AutofillType(ADDRESS_HOME_LINE2),
                       ASCIIToUTF16("unit 5"));
  home_profile.SetInfo(AutofillType(ADDRESS_HOME_CITY),
                       ASCIIToUTF16("Los Angeles"));
  home_profile.SetInfo(AutofillType(ADDRESS_HOME_STATE), ASCIIToUTF16("CA"));
  home_profile.SetInfo(AutofillType(ADDRESS_HOME_ZIP), ASCIIToUTF16("90025"));
  home_profile.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("US"));
  home_profile.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                       ASCIIToUTF16("18181234567"));
  home_profile.SetInfo(AutofillType(PHONE_FAX_WHOLE_NUMBER),
                       ASCIIToUTF16("1915243678"));

  Time pre_creation_time = Time::Now();
  EXPECT_TRUE(db.AddAutofillProfile(home_profile));
  Time post_creation_time = Time::Now();

  // Get the 'Home' profile.
  AutofillProfile* db_profile;
  ASSERT_TRUE(db.GetAutofillProfile(home_profile.guid(), &db_profile));
  EXPECT_EQ(home_profile, *db_profile);
  sql::Statement s_home(db.db_.GetUniqueStatement(
      "SELECT date_modified "
      "FROM autofill_profiles WHERE guid=?"));
  s_home.BindString(0, home_profile.guid());
  ASSERT_TRUE(s_home);
  ASSERT_TRUE(s_home.Step());
  EXPECT_GE(s_home.ColumnInt64(0), pre_creation_time.ToTimeT());
  EXPECT_LE(s_home.ColumnInt64(0), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_home.Step());
  delete db_profile;

  // Add a 'Billing' profile.
  AutofillProfile billing_profile = home_profile;
  billing_profile.set_guid(guid::GenerateGUID());
  billing_profile.SetInfo(AutofillType(ADDRESS_HOME_LINE1),
                          ASCIIToUTF16("5678 Bottom Street"));
  billing_profile.SetInfo(AutofillType(ADDRESS_HOME_LINE2),
                          ASCIIToUTF16("suite 3"));

  pre_creation_time = Time::Now();
  EXPECT_TRUE(db.AddAutofillProfile(billing_profile));
  post_creation_time = Time::Now();

  // Get the 'Billing' profile.
  ASSERT_TRUE(db.GetAutofillProfile(billing_profile.guid(), &db_profile));
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing);
  ASSERT_TRUE(s_billing.Step());
  EXPECT_GE(s_billing.ColumnInt64(0), pre_creation_time.ToTimeT());
  EXPECT_LE(s_billing.ColumnInt64(0), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_billing.Step());
  delete db_profile;

  // Update the 'Billing' profile, name only.
  billing_profile.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("Jane"));
  Time pre_modification_time = Time::Now();
  EXPECT_TRUE(db.UpdateAutofillProfile(billing_profile));
  Time post_modification_time = Time::Now();
  ASSERT_TRUE(db.GetAutofillProfile(billing_profile.guid(), &db_profile));
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing_updated(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing_updated.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing_updated);
  ASSERT_TRUE(s_billing_updated.Step());
  EXPECT_GE(s_billing_updated.ColumnInt64(0),
            pre_modification_time.ToTimeT());
  EXPECT_LE(s_billing_updated.ColumnInt64(0),
            post_modification_time.ToTimeT());
  EXPECT_FALSE(s_billing_updated.Step());
  delete db_profile;

  // Update the 'Billing' profile.
  billing_profile.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("Janice"));
  billing_profile.SetInfo(AutofillType(NAME_MIDDLE), ASCIIToUTF16("C."));
  billing_profile.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("Joplin"));
  billing_profile.SetInfo(AutofillType(EMAIL_ADDRESS),
                          ASCIIToUTF16("jane@singer.com"));
  billing_profile.SetInfo(AutofillType(COMPANY_NAME), ASCIIToUTF16("Indy"));
  billing_profile.SetInfo(AutofillType(ADDRESS_HOME_LINE1),
                          ASCIIToUTF16("Open Road"));
  billing_profile.SetInfo(AutofillType(ADDRESS_HOME_LINE2),
                          ASCIIToUTF16("Route 66"));
  billing_profile.SetInfo(AutofillType(ADDRESS_HOME_CITY),
                          ASCIIToUTF16("NFA"));
  billing_profile.SetInfo(AutofillType(ADDRESS_HOME_STATE), ASCIIToUTF16("NY"));
  billing_profile.SetInfo(AutofillType(ADDRESS_HOME_ZIP),
                          ASCIIToUTF16("10011"));
  billing_profile.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY),
                          ASCIIToUTF16("United States"));
  billing_profile.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                          ASCIIToUTF16("18181230000"));
  billing_profile.SetInfo(AutofillType(PHONE_FAX_WHOLE_NUMBER),
                          ASCIIToUTF16("1915240000"));
  Time pre_modification_time_2 = Time::Now();
  EXPECT_TRUE(db.UpdateAutofillProfile(billing_profile));
  Time post_modification_time_2 = Time::Now();
  ASSERT_TRUE(db.GetAutofillProfile(billing_profile.guid(), &db_profile));
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing_updated_2(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing_updated_2.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing_updated_2);
  ASSERT_TRUE(s_billing_updated_2.Step());
  EXPECT_GE(s_billing_updated_2.ColumnInt64(0),
            pre_modification_time_2.ToTimeT());
  EXPECT_LE(s_billing_updated_2.ColumnInt64(0),
            post_modification_time_2.ToTimeT());
  EXPECT_FALSE(s_billing_updated_2.Step());
  delete db_profile;

  // Remove the 'Billing' profile.
  EXPECT_TRUE(db.RemoveAutofillProfile(billing_profile.guid()));
  EXPECT_FALSE(db.GetAutofillProfile(billing_profile.guid(), &db_profile));
}

TEST_F(WebDatabaseTest, CreditCard) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Add a 'Work' credit card.
  CreditCard work_creditcard;
  work_creditcard.SetInfo(AutofillType(CREDIT_CARD_NAME),
                          ASCIIToUTF16("Jack Torrance"));
  work_creditcard.SetInfo(AutofillType(CREDIT_CARD_NUMBER),
                          ASCIIToUTF16("1234567890123456"));
  work_creditcard.SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH),
                          ASCIIToUTF16("04"));
  work_creditcard.SetInfo(AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                          ASCIIToUTF16("2013"));

  Time pre_creation_time = Time::Now();
  EXPECT_TRUE(db.AddCreditCard(work_creditcard));
  Time post_creation_time = Time::Now();

  // Get the 'Work' credit card.
  CreditCard* db_creditcard;
  ASSERT_TRUE(db.GetCreditCard(work_creditcard.guid(), &db_creditcard));
  EXPECT_EQ(work_creditcard, *db_creditcard);
  sql::Statement s_work(db.db_.GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified "
      "FROM credit_cards WHERE guid=?"));
  s_work.BindString(0, work_creditcard.guid());
  ASSERT_TRUE(s_work);
  ASSERT_TRUE(s_work.Step());
  EXPECT_GE(s_work.ColumnInt64(5), pre_creation_time.ToTimeT());
  EXPECT_LE(s_work.ColumnInt64(5), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_work.Step());
  delete db_creditcard;

  // Add a 'Target' credit card.
  CreditCard target_creditcard;
  target_creditcard.SetInfo(AutofillType(CREDIT_CARD_NAME),
                            ASCIIToUTF16("Jack Torrance"));
  target_creditcard.SetInfo(AutofillType(CREDIT_CARD_NUMBER),
                            ASCIIToUTF16("1111222233334444"));
  target_creditcard.SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH),
                            ASCIIToUTF16("06"));
  target_creditcard.SetInfo(AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                            ASCIIToUTF16("2012"));

  pre_creation_time = Time::Now();
  EXPECT_TRUE(db.AddCreditCard(target_creditcard));
  post_creation_time = Time::Now();
  ASSERT_TRUE(db.GetCreditCard(target_creditcard.guid(), &db_creditcard));
  EXPECT_EQ(target_creditcard, *db_creditcard);
  sql::Statement s_target(db.db_.GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified "
      "FROM credit_cards WHERE guid=?"));
  s_target.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_target);
  ASSERT_TRUE(s_target.Step());
  EXPECT_GE(s_target.ColumnInt64(5), pre_creation_time.ToTimeT());
  EXPECT_LE(s_target.ColumnInt64(5), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_target.Step());
  delete db_creditcard;

  // Update the 'Target' credit card.
  target_creditcard.SetInfo(AutofillType(CREDIT_CARD_NAME),
                            ASCIIToUTF16("Charles Grady"));
  Time pre_modification_time = Time::Now();
  EXPECT_TRUE(db.UpdateCreditCard(target_creditcard));
  Time post_modification_time = Time::Now();
  ASSERT_TRUE(db.GetCreditCard(target_creditcard.guid(), &db_creditcard));
  EXPECT_EQ(target_creditcard, *db_creditcard);
  sql::Statement s_target_updated(db.db_.GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified "
      "FROM credit_cards WHERE guid=?"));
  s_target_updated.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_target_updated);
  ASSERT_TRUE(s_target_updated.Step());
  EXPECT_GE(s_target_updated.ColumnInt64(5), pre_modification_time.ToTimeT());
  EXPECT_LE(s_target_updated.ColumnInt64(5), post_modification_time.ToTimeT());
  EXPECT_FALSE(s_target_updated.Step());
  delete db_creditcard;

  // Remove the 'Target' credit card.
  EXPECT_TRUE(db.RemoveCreditCard(target_creditcard.guid()));
  EXPECT_FALSE(db.GetCreditCard(target_creditcard.guid(), &db_creditcard));
}

TEST_F(WebDatabaseTest, UpdateAutofillProfile) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Add a profile to the db.
  AutofillProfile profile;
  profile.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("John"));
  profile.SetInfo(AutofillType(NAME_MIDDLE), ASCIIToUTF16("Q."));
  profile.SetInfo(AutofillType(NAME_LAST), ASCIIToUTF16("Smith"));
  profile.SetInfo(AutofillType(EMAIL_ADDRESS), ASCIIToUTF16("js@example.com"));
  profile.SetInfo(AutofillType(COMPANY_NAME), ASCIIToUTF16("Google"));
  profile.SetInfo(AutofillType(ADDRESS_HOME_LINE1),
                  ASCIIToUTF16("1234 Apple Way"));
  profile.SetInfo(AutofillType(ADDRESS_HOME_LINE2), ASCIIToUTF16("unit 5"));
  profile.SetInfo(AutofillType(ADDRESS_HOME_CITY), ASCIIToUTF16("Los Angeles"));
  profile.SetInfo(AutofillType(ADDRESS_HOME_STATE), ASCIIToUTF16("CA"));
  profile.SetInfo(AutofillType(ADDRESS_HOME_ZIP), ASCIIToUTF16("90025"));
  profile.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("US"));
  profile.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                  ASCIIToUTF16("18181234567"));
  profile.SetInfo(AutofillType(PHONE_FAX_WHOLE_NUMBER),
                  ASCIIToUTF16("1915243678"));
  db.AddAutofillProfile(profile);

  // Set a mocked value for the profile's creation time.
  const time_t mock_creation_date = Time::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(db.db_.GetUniqueStatement(
      "UPDATE autofill_profiles SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date);
  s_mock_creation_date.BindInt64(0, mock_creation_date);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the profile.
  AutofillProfile* tmp_profile;
  ASSERT_TRUE(db.GetAutofillProfile(profile.guid(), &tmp_profile));
  scoped_ptr<AutofillProfile> db_profile(tmp_profile);
  EXPECT_EQ(profile, *db_profile);
  sql::Statement s_original(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_original);
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(mock_creation_date, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update the profile and save the update to the database.
  // The modification date should change to reflect the update.
  profile.SetInfo(AutofillType(EMAIL_ADDRESS), ASCIIToUTF16("js@smith.xyz"));
  db.UpdateAutofillProfile(profile);

  // Get the profile.
  ASSERT_TRUE(db.GetAutofillProfile(profile.guid(), &tmp_profile));
  db_profile.reset(tmp_profile);
  EXPECT_EQ(profile, *db_profile);
  sql::Statement s_updated(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_updated);
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(mock_creation_date, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());

  // Set a mocked value for the profile's modification time.
  const time_t mock_modification_date = Time::Now().ToTimeT() - 7;
  sql::Statement s_mock_modification_date(db.db_.GetUniqueStatement(
      "UPDATE autofill_profiles SET date_modified = ?"));
  ASSERT_TRUE(s_mock_modification_date);
  s_mock_modification_date.BindInt64(0, mock_modification_date);
  ASSERT_TRUE(s_mock_modification_date.Run());

  // Finally, call into |UpdateAutofillProfile()| without changing the profile.
  // The modification date should not change.
  db.UpdateAutofillProfile(profile);

  // Get the profile.
  ASSERT_TRUE(db.GetAutofillProfile(profile.guid(), &tmp_profile));
  db_profile.reset(tmp_profile);
  EXPECT_EQ(profile, *db_profile);
  sql::Statement s_unchanged(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_unchanged);
  ASSERT_TRUE(s_unchanged.Step());
  EXPECT_EQ(mock_modification_date, s_unchanged.ColumnInt64(0));
  EXPECT_FALSE(s_unchanged.Step());
}

TEST_F(WebDatabaseTest, UpdateCreditCard) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Add a credit card to the db.
  CreditCard credit_card;
  credit_card.SetInfo(AutofillType(CREDIT_CARD_NAME),
                      ASCIIToUTF16("Jack Torrance"));
  credit_card.SetInfo(AutofillType(CREDIT_CARD_NUMBER),
                      ASCIIToUTF16("1234567890123456"));
  credit_card.SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH),
                      ASCIIToUTF16("04"));
  credit_card.SetInfo(AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                      ASCIIToUTF16("2013"));
  db.AddCreditCard(credit_card);

  // Set a mocked value for the credit card's creation time.
  const time_t mock_creation_date = Time::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(db.db_.GetUniqueStatement(
      "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date);
  s_mock_creation_date.BindInt64(0, mock_creation_date);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the credit card.
  CreditCard* tmp_credit_card;
  ASSERT_TRUE(db.GetCreditCard(credit_card.guid(), &tmp_credit_card));
  scoped_ptr<CreditCard> db_credit_card(tmp_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_original(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_original);
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(mock_creation_date, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update the credit card and save the update to the database.
  // The modification date should change to reflect the update.
  credit_card.SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH), ASCIIToUTF16("01"));
  db.UpdateCreditCard(credit_card);

  // Get the credit card.
  ASSERT_TRUE(db.GetCreditCard(credit_card.guid(), &tmp_credit_card));
  db_credit_card.reset(tmp_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_updated(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_updated);
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(mock_creation_date, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());

  // Set a mocked value for the credit card's modification time.
  const time_t mock_modification_date = Time::Now().ToTimeT() - 7;
  sql::Statement s_mock_modification_date(db.db_.GetUniqueStatement(
      "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_modification_date);
  s_mock_modification_date.BindInt64(0, mock_modification_date);
  ASSERT_TRUE(s_mock_modification_date.Run());

  // Finally, call into |UpdateCreditCard()| without changing the credit card.
  // The modification date should not change.
  db.UpdateCreditCard(credit_card);

  // Get the profile.
  ASSERT_TRUE(db.GetCreditCard(credit_card.guid(), &tmp_credit_card));
  db_credit_card.reset(tmp_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_unchanged(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_unchanged);
  ASSERT_TRUE(s_unchanged.Step());
  EXPECT_EQ(mock_modification_date, s_unchanged.ColumnInt64(0));
  EXPECT_FALSE(s_unchanged.Step());
}

TEST_F(WebDatabaseTest, RemoveAutofillProfilesAndCreditCardsModifiedBetween) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Populate the autofill_profiles and credit_cards tables.
  ASSERT_TRUE(db.db_.Execute(
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000000', 11);"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000001', 21);"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000002', 31);"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000003', 41);"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000004', 51);"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000005', 61);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000006', 17);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000007', 27);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000008', 37);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000009', 47);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000010', 57);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000011', 67);"));

  // Remove all entries modified in the bounded time range [17,41).
  db.RemoveAutofillProfilesAndCreditCardsModifiedBetween(
      base::Time::FromTimeT(17), base::Time::FromTimeT(41));
  sql::Statement s_autofill_profiles_bounded(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles_bounded);
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(11, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(41, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(51, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(61, s_autofill_profiles_bounded.ColumnInt64(0));
  EXPECT_FALSE(s_autofill_profiles_bounded.Step());
  sql::Statement s_credit_cards_bounded(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_bounded);
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(47, s_credit_cards_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(57, s_credit_cards_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(67, s_credit_cards_bounded.ColumnInt64(0));
  EXPECT_FALSE(s_credit_cards_bounded.Step());

  // Remove all entries modified on or after time 51 (unbounded range).
  db.RemoveAutofillProfilesAndCreditCardsModifiedBetween(
      base::Time::FromTimeT(51), base::Time());
  sql::Statement s_autofill_profiles_unbounded(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles_unbounded);
  ASSERT_TRUE(s_autofill_profiles_unbounded.Step());
  EXPECT_EQ(11, s_autofill_profiles_unbounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_unbounded.Step());
  EXPECT_EQ(41, s_autofill_profiles_unbounded.ColumnInt64(0));
  EXPECT_FALSE(s_autofill_profiles_unbounded.Step());
  sql::Statement s_credit_cards_unbounded(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_unbounded);
  ASSERT_TRUE(s_credit_cards_unbounded.Step());
  EXPECT_EQ(47, s_credit_cards_unbounded.ColumnInt64(0));
  EXPECT_FALSE(s_credit_cards_unbounded.Step());

  // Remove all remaining entries.
  db.RemoveAutofillProfilesAndCreditCardsModifiedBetween(base::Time(),
                                                         base::Time());
  sql::Statement s_autofill_profiles_empty(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles_empty);
  EXPECT_FALSE(s_autofill_profiles_empty.Step());
  sql::Statement s_credit_cards_empty(db.db_.GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_empty);
  EXPECT_FALSE(s_credit_cards_empty.Step());
}

TEST_F(WebDatabaseTest, Autofill_GetAllAutofillEntries_NoResults) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(db.GetAllAutofillEntries(&entries));

  EXPECT_EQ(0U, entries.size());
}

TEST_F(WebDatabaseTest, Autofill_GetAllAutofillEntries_OneResult) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillChangeList changes;
  std::map<std::string, std::vector<base::Time> > name_value_times_map;

  time_t start = 0;
  std::vector<base::Time> timestamps1;
  EXPECT_TRUE(db.AddFormFieldValueTime(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Superman"),
                string16(),
                0,
                false),
      &changes,
      Time::FromTimeT(start)));
  timestamps1.push_back(Time::FromTimeT(start));
  std::string key1("NameSuperman");
  name_value_times_map.insert(std::pair<std::string,
    std::vector<base::Time> > (key1, timestamps1));

  AutofillEntrySet expected_entries(CompareAutofillEntries);
  AutofillKey ak1(ASCIIToUTF16("Name"), ASCIIToUTF16("Superman"));
  AutofillEntry ae1(ak1, timestamps1);

  expected_entries.insert(ae1);

  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(db.GetAllAutofillEntries(&entries));
  AutofillEntrySet entry_set(entries.begin(), entries.end(),
    CompareAutofillEntries);

  // make sure the lists of entries match
  ASSERT_EQ(expected_entries.size(), entry_set.size());
  AutofillEntrySetIterator it;
  for (it = entry_set.begin(); it != entry_set.end(); it++) {
    expected_entries.erase(*it);
  }

  EXPECT_EQ(0U, expected_entries.size());
}

TEST_F(WebDatabaseTest, Autofill_GetAllAutofillEntries_TwoDistinct) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillChangeList changes;
  std::map<std::string, std::vector<base::Time> > name_value_times_map;
  time_t start = 0;

  std::vector<base::Time> timestamps1;
  EXPECT_TRUE(db.AddFormFieldValueTime(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Superman"),
                string16(),
                0,
                false),
      &changes,
      Time::FromTimeT(start)));
  timestamps1.push_back(Time::FromTimeT(start));
  std::string key1("NameSuperman");
  name_value_times_map.insert(std::pair<std::string,
    std::vector<base::Time> > (key1, timestamps1));

  start++;
  std::vector<base::Time> timestamps2;
  EXPECT_TRUE(db.AddFormFieldValueTime(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Clark Kent"),
                string16(),
                0,
                false),
      &changes,
      Time::FromTimeT(start)));
  timestamps2.push_back(Time::FromTimeT(start));
  std::string key2("NameClark Kent");
  name_value_times_map.insert(std::pair<std::string,
    std::vector<base::Time> > (key2, timestamps2));

  AutofillEntrySet expected_entries(CompareAutofillEntries);
  AutofillKey ak1(ASCIIToUTF16("Name"), ASCIIToUTF16("Superman"));
  AutofillKey ak2(ASCIIToUTF16("Name"), ASCIIToUTF16("Clark Kent"));
  AutofillEntry ae1(ak1, timestamps1);
  AutofillEntry ae2(ak2, timestamps2);

  expected_entries.insert(ae1);
  expected_entries.insert(ae2);

  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(db.GetAllAutofillEntries(&entries));
  AutofillEntrySet entry_set(entries.begin(), entries.end(),
    CompareAutofillEntries);

  // make sure the lists of entries match
  ASSERT_EQ(expected_entries.size(), entry_set.size());
  AutofillEntrySetIterator it;
  for (it = entry_set.begin(); it != entry_set.end(); it++) {
    expected_entries.erase(*it);
  }

  EXPECT_EQ(0U, expected_entries.size());
}

TEST_F(WebDatabaseTest, Autofill_GetAllAutofillEntries_TwoSame) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  AutofillChangeList changes;
  std::map<std::string, std::vector<base::Time> > name_value_times_map;

  time_t start = 0;
  std::vector<base::Time> timestamps;
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(db.AddFormFieldValueTime(
        FormField(string16(),
                  ASCIIToUTF16("Name"),
                  ASCIIToUTF16("Superman"),
                  string16(),
                  0,
                  false),
        &changes,
        Time::FromTimeT(start)));
    timestamps.push_back(Time::FromTimeT(start));
    start++;
  }

  std::string key("NameSuperman");
  name_value_times_map.insert(std::pair<std::string,
      std::vector<base::Time> > (key, timestamps));

  AutofillEntrySet expected_entries(CompareAutofillEntries);
  AutofillKey ak1(ASCIIToUTF16("Name"), ASCIIToUTF16("Superman"));
  AutofillEntry ae1(ak1, timestamps);

  expected_entries.insert(ae1);

  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(db.GetAllAutofillEntries(&entries));
  AutofillEntrySet entry_set(entries.begin(), entries.end(),
    CompareAutofillEntries);

  // make sure the lists of entries match
  ASSERT_EQ(expected_entries.size(), entry_set.size());
  AutofillEntrySetIterator it;
  for (it = entry_set.begin(); it != entry_set.end(); it++) {
    expected_entries.erase(*it);
  }

  EXPECT_EQ(0U, expected_entries.size());
}

// The WebDatabaseMigrationTest encapsulates testing of database migrations.
// Specifically, these tests are intended to exercise any schema changes in
// the WebDatabase and data migrations that occur in
// |WebDatabase::MigrateOldVersionsAsNeeded()|.
class WebDatabaseMigrationTest : public testing::Test {
 public:
  WebDatabaseMigrationTest() {}
  virtual ~WebDatabaseMigrationTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  // Current tested version number.  When adding a migration in
  // |WebDatabase::MigrateOldVersionsAsNeeded()| and changing the version number
  // |kCurrentVersionNumber| this value should change to reflect the new version
  // number and a new migration test added below.
  static const int kCurrentTestedVersionNumber;

  FilePath GetDatabasePath() {
    const FilePath::CharType kWebDatabaseFilename[] =
        FILE_PATH_LITERAL("TestWebDatabase.sqlite3");
    return temp_dir_.path().Append(FilePath(kWebDatabaseFilename));
  }

  // The textual contents of |file| are read from
  // "chrome/test/data/web_database" and returned in the string |contents|.
  // Returns true if the file exists and is read successfully, false otherwise.
  bool GetWebDatabaseData(const FilePath& file, std::string* contents) {
    FilePath path = ui_test_utils::GetTestFilePath(
        FilePath(FILE_PATH_LITERAL("web_database")), file);
    return file_util::PathExists(path) &&
        file_util::ReadFileToString(path, contents);
  }

  static int VersionFromConnection(sql::Connection* connection) {
    // Get version.
    sql::Statement s(connection->GetUniqueStatement(
        "SELECT value FROM meta WHERE key='version'"));
    if (!s.Step())
      return 0;
    return s.ColumnInt(0);
  }

  // The sql files located in "chrome/test/data/web_database" were generated by
  // launching the Chromium application prior to schema change, then using the
  // sqlite3 command-line application to dump the contents of the "Web Data"
  // database.
  // Like this:
  //   > .output version_nn.sql
  //   > .dump
  void LoadDatabase(const FilePath::StringType& file);

  // Assertion testing for migrating from version 27 and 28.
  void MigrateVersion28Assertions();

 private:
  ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseMigrationTest);
};

const int WebDatabaseMigrationTest::kCurrentTestedVersionNumber = 35;

void WebDatabaseMigrationTest::LoadDatabase(const FilePath::StringType& file) {
  std::string contents;
  ASSERT_TRUE(GetWebDatabaseData(FilePath(file), &contents));

  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.Execute(contents.data()));
}

void WebDatabaseMigrationTest::MigrateVersion28Assertions() {
  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Make sure supports_instant was dropped and instant_url was added.
    EXPECT_FALSE(connection.DoesColumnExist("keywords", "supports_instant"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "instant_url"));

    // Check that instant_url is empty.
    std::string stmt = "SELECT instant_url FROM keywords";
    sql::Statement s(connection.GetUniqueStatement(stmt.c_str()));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(std::string(), s.ColumnString(0));

    // Verify the data made it over.
    stmt = "SELECT id, short_name, keyword, favicon_url, url, "
        "show_in_default_list, safe_for_autoreplace, originating_url, "
        "date_created, usage_count, input_encodings, suggest_url, "
        "prepopulate_id, autogenerate_keyword, logo_id, created_by_policy, "
        "instant_url FROM keywords";
    sql::Statement s2(connection.GetUniqueStatement(stmt.c_str()));
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ(2, s2.ColumnInt(0));
    EXPECT_EQ("Google", s2.ColumnString(1));
    EXPECT_EQ("google.com", s2.ColumnString(2));
    EXPECT_EQ("http://www.google.com/favicon.ico", s2.ColumnString(3));
    EXPECT_EQ("{google:baseURL}search?{google:RLZ}{google:acceptedSuggestion}"\
        "{google:originalQueryForSuggestion}sourceid=chrome&ie={inputEncoding}"\
        "&q={searchTerms}",
        s2.ColumnString(4));
    EXPECT_EQ(1, s2.ColumnInt(5));
    EXPECT_EQ(1, s2.ColumnInt(6));
    EXPECT_EQ(std::string(), s2.ColumnString(7));
    EXPECT_EQ(0, s2.ColumnInt(8));
    EXPECT_EQ(0, s2.ColumnInt(9));
    EXPECT_EQ(std::string("UTF-8"), s2.ColumnString(10));
    EXPECT_EQ(std::string("{google:baseSuggestURL}search?client=chrome&hl="
                          "{language}&q={searchTerms}"), s2.ColumnString(11));
    EXPECT_EQ(1, s2.ColumnInt(12));
    EXPECT_EQ(1, s2.ColumnInt(13));
    EXPECT_EQ(6245, s2.ColumnInt(14));
    EXPECT_EQ(0, s2.ColumnInt(15));
    EXPECT_EQ(0, s2.ColumnInt(16));
    EXPECT_EQ(std::string(), s2.ColumnString(17));
  }
}

// Tests that the all migrations from an empty database succeed.
TEST_F(WebDatabaseMigrationTest, MigrateEmptyToCurrent) {
  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Check that expected tables are present.
    EXPECT_TRUE(connection.DoesTableExist("meta"));
    EXPECT_TRUE(connection.DoesTableExist("keywords"));
    EXPECT_TRUE(connection.DoesTableExist("logins"));
    EXPECT_TRUE(connection.DoesTableExist("web_app_icons"));
    EXPECT_TRUE(connection.DoesTableExist("web_apps"));
    EXPECT_TRUE(connection.DoesTableExist("autofill"));
    EXPECT_TRUE(connection.DoesTableExist("autofill_dates"));
    EXPECT_TRUE(connection.DoesTableExist("autofill_profiles"));
    EXPECT_TRUE(connection.DoesTableExist("credit_cards"));
    EXPECT_TRUE(connection.DoesTableExist("token_service"));
  }
}

// Tests that the |credit_card| table gets added to the schema for a version 22
// database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion22ToCurrent) {
  // This schema is taken from a build prior to the addition of the
  // |credit_card| table.  Version 22 of the schema.  Contrast this with the
  // corrupt version below.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_22.sql")));

  // Verify pre-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // No |credit_card| table prior to version 23.
    ASSERT_FALSE(connection.DoesColumnExist("credit_cards", "guid"));
    ASSERT_FALSE(
        connection.DoesColumnExist("credit_cards", "card_number_encrypted"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // |credit_card| table now exists.
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "guid"));
    EXPECT_TRUE(
        connection.DoesColumnExist("credit_cards", "card_number_encrypted"));
  }
}

// Tests that the |credit_card| table gets added to the schema for a corrupt
// version 22 database.  The corruption is that the |credit_cards| table exists
// but the schema version number was not set correctly to 23 or later.  This
// test exercises code introduced to fix bug http://crbug.com/50699 that
// resulted from the corruption.
TEST_F(WebDatabaseMigrationTest, MigrateVersion22CorruptedToCurrent) {
  // This schema is taken from a build after the addition of the |credit_card|
  // table.  Due to a bug in the migration logic the version is set incorrectly
  // to 22 (it should have been updated to 23 at least).
  ASSERT_NO_FATAL_FAILURE(
      LoadDatabase(FILE_PATH_LITERAL("version_22_corrupt.sql")));

  // Verify pre-conditions.  These are expectations for corrupt version 22 of
  // the database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns existing and not existing before current version.
    ASSERT_TRUE(connection.DoesColumnExist("credit_cards", "unique_id"));
    ASSERT_TRUE(
        connection.DoesColumnExist("credit_cards", "card_number_encrypted"));
    ASSERT_TRUE(connection.DoesColumnExist("keywords", "id"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords", "logo_id"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));


    // Columns existing and not existing before version 25.
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "unique_id"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "guid"));
    EXPECT_TRUE(
        connection.DoesColumnExist("credit_cards", "card_number_encrypted"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "id"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "logo_id"));
  }
}

// Tests that the |keywords| |logo_id| column gets added to the schema for a
// version 24 database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion24ToCurrent) {
  // This schema is taken from a build prior to the addition of the |keywords|
  // |logo_id| column.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_24.sql")));

  // Verify pre-conditions.  These are expectations for version 24 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns existing and not existing before current version.
    ASSERT_TRUE(connection.DoesColumnExist("keywords", "id"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords", "logo_id"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // |keywords| |logo_id| column should have been added.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "id"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "logo_id"));
  }
}

// Tests that the |keywords| |created_by_policy| column gets added to the schema
// for a version 25 database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion25ToCurrent) {
  // This schema is taken from a build prior to the addition of the |keywords|
  // |created_by_policy| column.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_25.sql")));

  // Verify pre-conditions.  These are expectations for version 25 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // |keywords| |logo_id| column should have been added.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "id"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "created_by_policy"));
  }
}

// Tests that the credit_cards.billing_address column is changed from a string
// to an int whilst preserving the associated billing address. This version of
// the test makes sure a stored label is converted to an ID.
TEST_F(WebDatabaseMigrationTest, MigrateVersion26ToCurrentStringLabels) {
  // This schema is taken from a build prior to the change of column type for
  // credit_cards.billing_address from string to int.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_26.sql")));

  // Verify pre-conditions. These are expectations for version 26 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns existing and not existing before current version.
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "billing_address"));

    std::string stmt = "INSERT INTO autofill_profiles"
      "(label, unique_id, first_name, middle_name, last_name, email,"
      " company_name, address_line_1, address_line_2, city, state, zipcode,"
      " country, phone, fax)"
      "VALUES ('Home',1,'','','','','','','','','','','','','')";
    sql::Statement s(connection.GetUniqueStatement(stmt.c_str()));
    ASSERT_TRUE(s.Run());

    // Insert a CC linked to an existing address.
    std::string stmt2 = "INSERT INTO credit_cards"
      "(label, unique_id, name_on_card, type, card_number,"
      " expiration_month, expiration_year, verification_code, billing_address,"
      " shipping_address, card_number_encrypted, verification_code_encrypted)"
      "VALUES ('label',2,'Jack','Visa','1234',2,2012,'','Home','','','')";
    sql::Statement s2(connection.GetUniqueStatement(stmt2.c_str()));
    ASSERT_TRUE(s2.Run());

    // |billing_address| is a string.
    std::string stmt3 = "SELECT billing_address FROM credit_cards";
    sql::Statement s3(connection.GetUniqueStatement(stmt3.c_str()));
    ASSERT_TRUE(s3.Step());
    EXPECT_EQ(s3.ColumnType(0), sql::COLUMN_TYPE_TEXT);
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "billing_address"));

    // Verify the credit card data is converted.
    sql::Statement s(connection.GetUniqueStatement(
        "SELECT guid, name_on_card, expiration_month, expiration_year, "
        "card_number_encrypted, date_modified "
        "FROM credit_cards"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ("Jack", s.ColumnString(1));
    EXPECT_EQ(2, s.ColumnInt(2));
    EXPECT_EQ(2012, s.ColumnInt(3));
    // Column 5 is encrypted number blob.
    // Column 6 is date_modified.
  }
}

// Tests that the credit_cards.billing_address column is changed from a string
// to an int whilst preserving the associated billing address. This version of
// the test makes sure a stored string ID is converted to an integer ID.
TEST_F(WebDatabaseMigrationTest, MigrateVersion26ToCurrentStringIDs) {
  // This schema is taken from a build prior to the change of column type for
  // credit_cards.billing_address from string to int.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_26.sql")));

  // Verify pre-conditions. These are expectations for version 26 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "billing_address"));

    std::string stmt = "INSERT INTO autofill_profiles"
      "(label, unique_id, first_name, middle_name, last_name, email,"
      " company_name, address_line_1, address_line_2, city, state, zipcode,"
      " country, phone, fax)"
      "VALUES ('Home',1,'','','','','','','','','','','','','')";
    sql::Statement s(connection.GetUniqueStatement(stmt.c_str()));
    ASSERT_TRUE(s.Run());

    // Insert a CC linked to an existing address.
    std::string stmt2 = "INSERT INTO credit_cards"
      "(label, unique_id, name_on_card, type, card_number,"
      " expiration_month, expiration_year, verification_code, billing_address,"
      " shipping_address, card_number_encrypted, verification_code_encrypted)"
      "VALUES ('label',2,'Jack','Visa','1234',2,2012,'','1','','','')";
    sql::Statement s2(connection.GetUniqueStatement(stmt2.c_str()));
    ASSERT_TRUE(s2.Run());

    // |billing_address| is a string.
    std::string stmt3 = "SELECT billing_address FROM credit_cards";
    sql::Statement s3(connection.GetUniqueStatement(stmt3.c_str()));
    ASSERT_TRUE(s3.Step());
    EXPECT_EQ(s3.ColumnType(0), sql::COLUMN_TYPE_TEXT);
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // |keywords| |logo_id| column should have been added.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "id"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "created_by_policy"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "billing_address"));

    // Verify the credit card data is converted.
    sql::Statement s(connection.GetUniqueStatement(
        "SELECT guid, name_on_card, expiration_month, expiration_year, "
        "card_number_encrypted, date_modified "
        "FROM credit_cards"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ("Jack", s.ColumnString(1));
    EXPECT_EQ(2, s.ColumnInt(2));
    EXPECT_EQ(2012, s.ColumnInt(3));
    // Column 5 is encrypted credit card number blob.
    // Column 6 is date_modified.
  }
}

// Tests migration from 27->current. This test is now the same as 28->current
// as the column added in 28 was nuked in 29.
TEST_F(WebDatabaseMigrationTest, MigrateVersion27ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_27.sql")));

  // Verify pre-conditions. These are expectations for version 28 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    ASSERT_FALSE(connection.DoesColumnExist("keywords", "supports_instant"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords", "instant_url"));
  }

  MigrateVersion28Assertions();
}

// Makes sure instant_url is added correctly to keywords.
TEST_F(WebDatabaseMigrationTest, MigrateVersion28ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_28.sql")));

  // Verify pre-conditions. These are expectations for version 28 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    ASSERT_TRUE(connection.DoesColumnExist("keywords", "supports_instant"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords", "instant_url"));
  }

  MigrateVersion28Assertions();
}

// Makes sure date_modified is added correctly to autofill_profiles and
// credit_cards.
TEST_F(WebDatabaseMigrationTest, MigrateVersion29ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_29.sql")));

  // Verify pre-conditions.  These are expectations for version 29 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles",
                                            "date_modified"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards",
                                            "date_modified"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  Time pre_creation_time = Time::Now();
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }
  Time post_creation_time = Time::Now();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Check that the columns were created.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "date_modified"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards",
                                           "date_modified"));

    sql::Statement s_profiles(connection.GetUniqueStatement(
        "SELECT date_modified FROM autofill_profiles "));
    ASSERT_TRUE(s_profiles);
    while (s_profiles.Step()) {
      EXPECT_GE(s_profiles.ColumnInt64(0),
                pre_creation_time.ToTimeT());
      EXPECT_LE(s_profiles.ColumnInt64(0),
                post_creation_time.ToTimeT());
    }
    EXPECT_TRUE(s_profiles.Succeeded());

    sql::Statement s_credit_cards(connection.GetUniqueStatement(
        "SELECT date_modified FROM credit_cards "));
    ASSERT_TRUE(s_credit_cards);
    while (s_credit_cards.Step()) {
      EXPECT_GE(s_credit_cards.ColumnInt64(0),
                pre_creation_time.ToTimeT());
      EXPECT_LE(s_credit_cards.ColumnInt64(0),
                post_creation_time.ToTimeT());
    }
    EXPECT_TRUE(s_credit_cards.Succeeded());
  }
}

// Makes sure guids are added to autofill_profiles and credit_cards tables.
TEST_F(WebDatabaseMigrationTest, MigrateVersion30ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_30.sql")));

  // Verify pre-conditions. These are expectations for version 29 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "guid"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "guid"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    ASSERT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));
    ASSERT_TRUE(connection.DoesColumnExist("credit_cards", "guid"));

    // Check that guids are non-null, non-empty, conforms to guid format, and
    // are different.
    sql::Statement s(
        connection.GetUniqueStatement("SELECT guid FROM autofill_profiles"));

    ASSERT_TRUE(s.Step());
    std::string guid1 = s.ColumnString(0);
    EXPECT_TRUE(guid::IsValidGUID(guid1));

    ASSERT_TRUE(s.Step());
    std::string guid2 = s.ColumnString(0);
    EXPECT_TRUE(guid::IsValidGUID(guid2));

    EXPECT_NE(guid1, guid2);
  }
}

// Removes unique IDs and make GUIDs the primary key.  Also removes unused
// columns.
TEST_F(WebDatabaseMigrationTest, MigrateVersion31ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_31.sql")));

  // Verify pre-conditions. These are expectations for version 30 of the
  // database.
  AutofillProfile profile;
  string16 profile_label;
  int profile_unique_id = 0;
  int64 profile_date_modified = 0;
  CreditCard credit_card;
  string16 cc_label;
  int cc_unique_id = 0;
  std::string cc_number_encrypted;
  int64 cc_date_modified = 0;
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Verify existence of columns we'll be changing.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "unique_id"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "unique_id"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "type"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "card_number"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards",
                                           "verification_code"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "billing_address"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "shipping_address"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards",
                                           "verification_code_encrypted"));

    // Fetch data in the database prior to migration.
    sql::Statement s1(
        connection.GetUniqueStatement(
            "SELECT label, unique_id, first_name, middle_name, last_name, "
            "email, company_name, address_line_1, address_line_2, city, state, "
            "zipcode, country, phone, fax, date_modified, guid "
            "FROM autofill_profiles"));
    ASSERT_TRUE(s1.Step());
    EXPECT_NO_FATAL_FAILURE(AutofillProfile31FromStatement(
        s1, &profile, &profile_label, &profile_unique_id,
        &profile_date_modified));

    sql::Statement s2(
        connection.GetUniqueStatement(
            "SELECT label, unique_id, name_on_card, type, card_number, "
            "expiration_month, expiration_year, verification_code, "
            "billing_address, shipping_address, card_number_encrypted, "
            "verification_code_encrypted, date_modified, guid "
            "FROM credit_cards"));
    ASSERT_TRUE(s2.Step());
    EXPECT_NO_FATAL_FAILURE(CreditCard31FromStatement(s2,
                                                      &credit_card,
                                                      &cc_label,
                                                      &cc_unique_id,
                                                      &cc_number_encrypted,
                                                      &cc_date_modified));

    EXPECT_NE(profile_unique_id, cc_unique_id);
    EXPECT_NE(profile.guid(), credit_card.guid());
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Verify existence of columns we'll be changing.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "unique_id"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "guid"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "unique_id"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "type"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "card_number"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards",
                                            "verification_code"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "billing_address"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards",
                                            "shipping_address"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards",
                                            "verification_code_encrypted"));

    // Verify data in the database after the migration.
    sql::Statement s1(
        connection.GetUniqueStatement(
            "SELECT guid, company_name, address_line_1, address_line_2, "
            "city, state, zipcode, country, date_modified "
            "FROM autofill_profiles"));
    ASSERT_TRUE(s1.Step());

    AutofillProfile profile_a;
    int64 profile_date_modified_a = 0;
    EXPECT_NO_FATAL_FAILURE(AutofillProfile33FromStatement(
        s1, &profile_a, &profile_date_modified_a));
    EXPECT_EQ(profile.guid(), profile_a.guid());
    EXPECT_EQ(profile.GetFieldText(AutofillType(COMPANY_NAME)),
              profile_a.GetFieldText(AutofillType(COMPANY_NAME)));
    EXPECT_EQ(profile.GetFieldText(AutofillType(ADDRESS_HOME_LINE1)),
              profile_a.GetFieldText(AutofillType(ADDRESS_HOME_LINE1)));
    EXPECT_EQ(profile.GetFieldText(AutofillType(ADDRESS_HOME_LINE2)),
              profile_a.GetFieldText(AutofillType(ADDRESS_HOME_LINE2)));
    EXPECT_EQ(profile.GetFieldText(AutofillType(ADDRESS_HOME_CITY)),
              profile_a.GetFieldText(AutofillType(ADDRESS_HOME_CITY)));
    EXPECT_EQ(profile.GetFieldText(AutofillType(ADDRESS_HOME_STATE)),
              profile_a.GetFieldText(AutofillType(ADDRESS_HOME_STATE)));
    EXPECT_EQ(profile.GetFieldText(AutofillType(ADDRESS_HOME_ZIP)),
              profile_a.GetFieldText(AutofillType(ADDRESS_HOME_ZIP)));
    EXPECT_EQ(profile.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY)),
              profile_a.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY)));
    EXPECT_EQ(profile_date_modified, profile_date_modified_a);

    sql::Statement s2(
        connection.GetUniqueStatement(
            "SELECT guid, name_on_card, expiration_month, "
            "expiration_year, card_number_encrypted, date_modified "
            "FROM credit_cards"));
    ASSERT_TRUE(s2.Step());

    CreditCard credit_card_a;
    string16 cc_label_a;
    std::string cc_number_encrypted_a;
    int64 cc_date_modified_a = 0;
    EXPECT_NO_FATAL_FAILURE(CreditCard32FromStatement(s2,
                                                      &credit_card_a,
                                                      &cc_number_encrypted_a,
                                                      &cc_date_modified_a));
    EXPECT_EQ(credit_card, credit_card_a);
    EXPECT_EQ(cc_label, cc_label_a);
    EXPECT_EQ(cc_number_encrypted, cc_number_encrypted_a);
    EXPECT_EQ(cc_date_modified, cc_date_modified_a);
  }
}

// Factor |autofill_profiles| address information separately from name, email,
// and phone.
TEST_F(WebDatabaseMigrationTest, MigrateVersion32ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_32.sql")));

  // Verify pre-conditions. These are expectations for version 32 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Verify existence of columns we'll be changing.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "label"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "first_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "middle_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "last_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "email"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "company_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "address_line_1"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "address_line_2"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "city"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "state"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "zipcode"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "country"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "phone"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "fax"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "date_modified"));

    EXPECT_FALSE(connection.DoesTableExist("autofill_profile_names"));
    EXPECT_FALSE(connection.DoesTableExist("autofill_profile_emails"));
    EXPECT_FALSE(connection.DoesTableExist("autofill_profile_phones"));

    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "label"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Verify changes to columns.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "label"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "first_name"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles",
                                            "middle_name"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "last_name"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "email"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "company_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "address_line_1"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "address_line_2"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "city"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "state"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "zipcode"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "country"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "phone"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "fax"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "date_modified"));

    // New "names" table.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_names", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_names",
                                           "first_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_names",
                                           "middle_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_names",
                                           "last_name"));

    // New "emails" table.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_emails", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_emails", "email"));

    // New "phones" table.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_phones", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_phones", "type"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_phones",
                                           "number"));

    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "label"));

    // Verify data in the database after the migration.
    sql::Statement s1(
        connection.GetUniqueStatement(
            "SELECT guid, company_name, address_line_1, address_line_2, "
            "city, state, zipcode, country, date_modified "
            "FROM autofill_profiles"));

    // John Doe.
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("00580526-FF81-EE2A-0546-1AC593A32E2F", s1.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Doe Enterprises"), s1.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("1 Main St"), s1.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Apt 1"), s1.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Altos"), s1.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s1.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94022"), s1.ColumnString16(6));
    EXPECT_EQ(ASCIIToUTF16("United States"), s1.ColumnString16(7));
    EXPECT_EQ(1297882100L, s1.ColumnInt64(8));

    // John P. Doe.
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("589636FD-9037-3053-200C-80ABC97D7344", s1.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Doe Enterprises"), s1.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("1 Main St"), s1.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Apt 1"), s1.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Altos"), s1.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s1.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94022"), s1.ColumnString16(6));
    EXPECT_EQ(ASCIIToUTF16("United States"), s1.ColumnString16(7));
    EXPECT_EQ(1297882100L, s1.ColumnInt64(8));

    // Dave Smith.
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("4C74A9D8-7EEE-423E-F9C2-E7FA70ED1396", s1.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("2 Main Street"), s1.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Altos"), s1.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s1.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94022"), s1.ColumnString16(6));
    EXPECT_EQ(ASCIIToUTF16("United States"), s1.ColumnString16(7));
    EXPECT_EQ(1297882100L, s1.ColumnInt64(8));

    // Dave Smith (Part 2).
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("722DF5C4-F74A-294A-46F0-31FFDED0D635", s1.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("2 Main St"), s1.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Altos"), s1.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s1.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94022"), s1.ColumnString16(6));
    EXPECT_EQ(ASCIIToUTF16("United States"), s1.ColumnString16(7));
    EXPECT_EQ(1297882100L, s1.ColumnInt64(8));

    // Alfred E Newman.
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("584282AC-5D21-8D73-A2DB-4F892EF61F3F", s1.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(6));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(7));
    EXPECT_EQ(1297882100L, s1.ColumnInt64(8));

    // 3 Main St.
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("9E5FE298-62C7-83DF-6293-381BC589183F", s1.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("3 Main St"), s1.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16(""), s1.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Altos"), s1.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s1.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94022"), s1.ColumnString16(6));
    EXPECT_EQ(ASCIIToUTF16("United States"), s1.ColumnString16(7));
    EXPECT_EQ(1297882100L, s1.ColumnInt64(8));

    // That should be all.
    EXPECT_FALSE(s1.Step());

    sql::Statement s2(
        connection.GetUniqueStatement(
            "SELECT guid, first_name, middle_name, last_name "
            "FROM autofill_profile_names"));

    // John Doe.
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("00580526-FF81-EE2A-0546-1AC593A32E2F", s2.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("John"), s2.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16(""), s2.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Doe"), s2.ColumnString16(3));

    // John P. Doe.
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("589636FD-9037-3053-200C-80ABC97D7344", s2.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("John"), s2.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("P."), s2.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Doe"), s2.ColumnString16(3));

    // Dave Smith.
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("4C74A9D8-7EEE-423E-F9C2-E7FA70ED1396", s2.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Dave"), s2.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16(""), s2.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Smith"), s2.ColumnString16(3));

    // Dave Smith (Part 2).
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("722DF5C4-F74A-294A-46F0-31FFDED0D635", s2.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Dave"), s2.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16(""), s2.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Smith"), s2.ColumnString16(3));

    // Alfred E Newman.
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("584282AC-5D21-8D73-A2DB-4F892EF61F3F", s2.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Alfred"), s2.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("E"), s2.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Newman"), s2.ColumnString16(3));

    // Note no name for 3 Main St.

    // Should be all.
    EXPECT_FALSE(s2.Step());

    sql::Statement s3(
        connection.GetUniqueStatement(
            "SELECT guid, email "
            "FROM autofill_profile_emails"));

    // John Doe.
    ASSERT_TRUE(s3.Step());
    EXPECT_EQ("00580526-FF81-EE2A-0546-1AC593A32E2F", s3.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("john@doe.com"), s3.ColumnString16(1));

    // John P. Doe.
    ASSERT_TRUE(s3.Step());
    EXPECT_EQ("589636FD-9037-3053-200C-80ABC97D7344", s3.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("john@doe.com"), s3.ColumnString16(1));

    // Note no email for 2 Main Street.
    // Note no email for 2 Main St.

    // Alfred E Newman.
    ASSERT_TRUE(s3.Step());
    EXPECT_EQ("584282AC-5D21-8D73-A2DB-4F892EF61F3F", s3.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("a@e.com"), s3.ColumnString16(1));

    // Note no email for 3 Main St.

    // Should be all.
    EXPECT_FALSE(s3.Step());

    sql::Statement s4(
        connection.GetUniqueStatement(
            "SELECT guid, type, number "
            "FROM autofill_profile_phones"));

    // John Doe phone.
    ASSERT_TRUE(s4.Step());
    EXPECT_EQ("00580526-FF81-EE2A-0546-1AC593A32E2F", s4.ColumnString(0));
    EXPECT_EQ(0, s4.ColumnInt(1));  // 0 means phone.
    EXPECT_EQ(ASCIIToUTF16("4151112222"), s4.ColumnString16(2));

    // John Doe fax.
    ASSERT_TRUE(s4.Step());
    EXPECT_EQ("00580526-FF81-EE2A-0546-1AC593A32E2F", s4.ColumnString(0));
    EXPECT_EQ(1, s4.ColumnInt(1)); // 1 means phone.
    EXPECT_EQ(ASCIIToUTF16("4153334444"), s4.ColumnString16(2));

    // John P. Doe phone.
    ASSERT_TRUE(s4.Step());
    EXPECT_EQ("589636FD-9037-3053-200C-80ABC97D7344", s4.ColumnString(0));
    EXPECT_EQ(0, s4.ColumnInt(1)); // 0 means phone.
    EXPECT_EQ(ASCIIToUTF16("4151112222"), s4.ColumnString16(2));

    // John P. Doe fax.
    ASSERT_TRUE(s4.Step());
    EXPECT_EQ("589636FD-9037-3053-200C-80ABC97D7344", s4.ColumnString(0));
    EXPECT_EQ(1, s4.ColumnInt(1)); // 1 means fax.
    EXPECT_EQ(ASCIIToUTF16("4153334444"), s4.ColumnString16(2));

    // Note no phone or fax for 2 Main Street.
    // Note no phone or fax for 2 Main St.
    // Note no phone or fax for Alfred E Newman.
    // Note no phone or fax for 3 Main St.

    // Should be all.
    EXPECT_FALSE(s4.Step());
  }
}

// Adds a column for the autofill profile's country code.
TEST_F(WebDatabaseMigrationTest, MigrateVersion33ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_33.sql")));

  // Verify pre-conditions. These are expectations for version 33 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles",
                                            "country_code"));

    // Check that the country value is the one we expect.
    sql::Statement s(
        connection.GetUniqueStatement("SELECT country FROM autofill_profiles"));

    ASSERT_TRUE(s.Step());
    std::string country = s.ColumnString(0);
    EXPECT_EQ("United States", country);
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    ASSERT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "country_code"));

    // Check that the country code is properly converted.
    sql::Statement s(connection.GetUniqueStatement(
        "SELECT country_code FROM autofill_profiles"));

    ASSERT_TRUE(s.Step());
    std::string country_code = s.ColumnString(0);
    EXPECT_EQ("US", country_code);
  }
}

// Cleans up bad country code "UK" in favor of good country code "GB".
TEST_F(WebDatabaseMigrationTest, MigrateVersion34ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_34.sql")));

  // Verify pre-conditions. These are expectations for version 34 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "country_code"));

    // Check that the country_code value is the one we expect.
    sql::Statement s(
        connection.GetUniqueStatement("SELECT country_code "
                                      "FROM autofill_profiles"));

    ASSERT_TRUE(s.Step());
    std::string country_code = s.ColumnString(0);
    EXPECT_EQ("UK", country_code);

    // Should have only one.
    ASSERT_FALSE(s.Step());
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    ASSERT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "country_code"));

    // Check that the country_code code is properly converted.
    sql::Statement s(connection.GetUniqueStatement(
        "SELECT country_code FROM autofill_profiles"));

    ASSERT_TRUE(s.Step());
    std::string country_code = s.ColumnString(0);
    EXPECT_EQ("GB", country_code);

    // Should have only one.
    ASSERT_FALSE(s.Step());
  }
}
