// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "base/string_number_conversions.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_paths.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "testing/gtest/include/gtest/gtest.h"
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

  return timestamps2.size() != 0U;
}

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
        base::Int64ToString(base::Time::Now().ToInternalValue()) +
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
  template_url.set_short_name(L"short_name");
  template_url.set_keyword(L"keyword");
  GURL favicon_url("http://favicon.url/");
  GURL originating_url("http://google.com/");
  template_url.SetFavIconURL(favicon_url);
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
  SetID(1, &template_url);

  EXPECT_TRUE(db.AddKeyword(template_url));

  std::vector<TemplateURL*> template_urls;
  EXPECT_TRUE(db.GetKeywords(&template_urls));

  EXPECT_EQ(1U, template_urls.size());
  const TemplateURL* restored_url = template_urls.front();

  EXPECT_EQ(template_url.short_name(), restored_url->short_name());

  EXPECT_EQ(template_url.keyword(), restored_url->keyword());

  EXPECT_FALSE(restored_url->autogenerate_keyword());

  EXPECT_TRUE(favicon_url == restored_url->GetFavIconURL());

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
  template_url.set_short_name(L"short_name");
  template_url.set_keyword(L"keyword");
  GURL favicon_url("http://favicon.url/");
  GURL originating_url("http://originating.url/");
  template_url.SetFavIconURL(favicon_url);
  template_url.SetURL("http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  template_url.set_show_in_default_list(true);
  template_url.SetSuggestionsURL("url2", 0, 0);
  SetID(1, &template_url);

  EXPECT_TRUE(db.AddKeyword(template_url));

  GURL originating_url2("http://originating.url/");
  template_url.set_originating_url(originating_url2);
  template_url.set_autogenerate_keyword(true);
  EXPECT_EQ(L"url", template_url.keyword());
  template_url.add_input_encoding("Shift_JIS");
  set_prepopulate_id(&template_url, 5);
  set_logo_id(&template_url, 2000);
  EXPECT_TRUE(db.UpdateKeyword(template_url));

  std::vector<TemplateURL*> template_urls;
  EXPECT_TRUE(db.GetKeywords(&template_urls));

  EXPECT_EQ(1U, template_urls.size());
  const TemplateURL* restored_url = template_urls.front();

  EXPECT_EQ(template_url.short_name(), restored_url->short_name());

  EXPECT_EQ(template_url.keyword(), restored_url->keyword());

  EXPECT_TRUE(restored_url->autogenerate_keyword());

  EXPECT_TRUE(favicon_url == restored_url->GetFavIconURL());

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

  delete restored_url;
}

TEST_F(WebDatabaseTest, KeywordWithNoFavicon) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  TemplateURL template_url;
  template_url.set_short_name(L"short_name");
  template_url.set_keyword(L"keyword");
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
  EXPECT_TRUE(!restored_url->GetFavIconURL().is_valid());
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
                0),
      &changes));
  std::vector<string16> v;
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(db.AddFormFieldValue(
        FormField(string16(),
                  ASCIIToUTF16("Name"),
                  ASCIIToUTF16("Clark Kent"),
                  string16(),
                  0),
        &changes));
  }
  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(db.AddFormFieldValue(
        FormField(string16(),
                  ASCIIToUTF16("Name"),
                  ASCIIToUTF16("Clark Sutter"),
                  string16(),
                  0),
        &changes));
  }
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(db.AddFormFieldValue(
        FormField(string16(),
                  ASCIIToUTF16("Favorite Color"),
                  ASCIIToUTF16("Green"),
                  string16(),
                  0),
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
                0),
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
                0),
      &pair_id, &count));
  EXPECT_EQ(0, count);

  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      FormField(string16(),
                ASCIIToUTF16("Favorite Color"),
                ASCIIToUTF16("Green"),
                string16(),
                0),
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
                0),
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
                                             0),
                                   &changes));
  EXPECT_TRUE(db.AddFormFieldValue(FormField(string16(),
                                             ASCIIToUTF16("blank"),
                                             ASCIIToUTF16(" "),
                                             string16(),
                                             0),
                                   &changes));
  EXPECT_TRUE(db.AddFormFieldValue(FormField(string16(),
                                             ASCIIToUTF16("blank"),
                                             ASCIIToUTF16("      "),
                                             string16(),
                                             0),
                                   &changes));
  EXPECT_TRUE(db.AddFormFieldValue(FormField(string16(),
                                             ASCIIToUTF16("blank"),
                                             kValue,
                                             string16(),
                                             0),
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
                0),
      &changes,
      t1));
  EXPECT_TRUE(db.AddFormFieldValueTime(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                ASCIIToUTF16("Superman"),
                string16(),
                0),
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
                0),
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
                0),
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
                  0);
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
                  0);
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
                  0);
  int64 pair_id;
  int count;
  ASSERT_TRUE(db.GetIDAndCountOfFormElement(field0, &pair_id, &count));
  EXPECT_LE(0, pair_id);
  EXPECT_EQ(1, count);

  FormField field1(string16(),
                  ASCIIToUTF16("foo"),
                  ASCIIToUTF16("bar1"),
                  string16(),
                  0);
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
                0),
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
                0),
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
                               0));
  elements.push_back(FormField(string16(),
                               ASCIIToUTF16("firstname"),
                               ASCIIToUTF16("Jane"),
                               string16(),
                               0));
  elements.push_back(FormField(string16(),
                               ASCIIToUTF16("lastname"),
                               ASCIIToUTF16("Smith"),
                               string16(),
                               0));
  elements.push_back(FormField(string16(),
                               ASCIIToUTF16("lastname"),
                               ASCIIToUTF16("Jones"),
                               string16(),
                               0));

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

TEST_F(WebDatabaseTest, AutoFillProfile) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Add a 'Home' profile.
  AutoFillProfile home_profile(ASCIIToUTF16("Home"), 17);
  home_profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("John"));
  home_profile.SetInfo(AutoFillType(NAME_MIDDLE), ASCIIToUTF16("Q."));
  home_profile.SetInfo(AutoFillType(NAME_LAST), ASCIIToUTF16("Smith"));
  home_profile.SetInfo(AutoFillType(EMAIL_ADDRESS),
                       ASCIIToUTF16("js@smith.xyz"));
  home_profile.SetInfo(AutoFillType(COMPANY_NAME), ASCIIToUTF16("Google"));
  home_profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE1),
                       ASCIIToUTF16("1234 Apple Way"));
  home_profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE2),
                       ASCIIToUTF16("unit 5"));
  home_profile.SetInfo(AutoFillType(ADDRESS_HOME_CITY),
                       ASCIIToUTF16("Los Angeles"));
  home_profile.SetInfo(AutoFillType(ADDRESS_HOME_STATE), ASCIIToUTF16("CA"));
  home_profile.SetInfo(AutoFillType(ADDRESS_HOME_ZIP), ASCIIToUTF16("90025"));
  home_profile.SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("US"));
  home_profile.SetInfo(AutoFillType(PHONE_HOME_WHOLE_NUMBER),
                       ASCIIToUTF16("18181234567"));
  home_profile.SetInfo(AutoFillType(PHONE_FAX_WHOLE_NUMBER),
                       ASCIIToUTF16("1915243678"));

  EXPECT_TRUE(db.AddAutoFillProfile(home_profile));

  // Get the 'Home' profile.
  AutoFillProfile* db_profile;
  ASSERT_TRUE(db.GetAutoFillProfileForLabel(ASCIIToUTF16("Home"), &db_profile));
  EXPECT_EQ(home_profile, *db_profile);
  delete db_profile;

  // Add a 'Billing' profile.
  AutoFillProfile billing_profile = home_profile;
  billing_profile.set_label(ASCIIToUTF16("Billing"));
  billing_profile.set_unique_id(13);
  billing_profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE1),
                          ASCIIToUTF16("5678 Bottom Street"));
  billing_profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE2),
                          ASCIIToUTF16("suite 3"));

  EXPECT_TRUE(db.AddAutoFillProfile(billing_profile));
  ASSERT_TRUE(db.GetAutoFillProfileForLabel(ASCIIToUTF16("Billing"),
                                            &db_profile));
  EXPECT_EQ(billing_profile, *db_profile);
  delete db_profile;

  // Update the 'Billing' profile.
  billing_profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Jane"));
  EXPECT_TRUE(db.UpdateAutoFillProfile(billing_profile));
  ASSERT_TRUE(db.GetAutoFillProfileForLabel(ASCIIToUTF16("Billing"),
                                            &db_profile));
  EXPECT_EQ(billing_profile, *db_profile);
  delete db_profile;

  // Remove the 'Billing' profile.
  EXPECT_TRUE(db.RemoveAutoFillProfile(billing_profile.unique_id()));
  EXPECT_FALSE(db.GetAutoFillProfileForLabel(ASCIIToUTF16("Billing"),
                                             &db_profile));
}

TEST_F(WebDatabaseTest, CreditCard) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  // Add a 'Work' credit card.
  CreditCard work_creditcard(ASCIIToUTF16("Work"), 13);
  work_creditcard.SetInfo(AutoFillType(CREDIT_CARD_NAME),
                          ASCIIToUTF16("Jack Torrance"));
  work_creditcard.SetInfo(AutoFillType(CREDIT_CARD_TYPE),
                          ASCIIToUTF16("Visa"));
  work_creditcard.SetInfo(AutoFillType(CREDIT_CARD_NUMBER),
                          ASCIIToUTF16("1234567890123456"));
  work_creditcard.SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH),
                          ASCIIToUTF16("04"));
  work_creditcard.SetInfo(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                          ASCIIToUTF16("2013"));
  work_creditcard.set_billing_address_id(1);

  EXPECT_TRUE(db.AddCreditCard(work_creditcard));

  // Get the 'Work' credit card.
  CreditCard* db_creditcard;
  ASSERT_TRUE(db.GetCreditCardForLabel(ASCIIToUTF16("Work"), &db_creditcard));
  EXPECT_EQ(work_creditcard, *db_creditcard);
  delete db_creditcard;

  // Add a 'Target' profile.
  CreditCard target_creditcard(ASCIIToUTF16("Target"), 7);
  target_creditcard.SetInfo(AutoFillType(CREDIT_CARD_NAME),
                            ASCIIToUTF16("Jack Torrance"));
  target_creditcard.SetInfo(AutoFillType(CREDIT_CARD_TYPE),
                            ASCIIToUTF16("Mastercard"));
  target_creditcard.SetInfo(AutoFillType(CREDIT_CARD_NUMBER),
                            ASCIIToUTF16("1111222233334444"));
  target_creditcard.SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH),
                            ASCIIToUTF16("06"));
  target_creditcard.SetInfo(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                            ASCIIToUTF16("2012"));
  target_creditcard.set_billing_address_id(1);

  EXPECT_TRUE(db.AddCreditCard(target_creditcard));
  ASSERT_TRUE(db.GetCreditCardForLabel(ASCIIToUTF16("Target"),
                                       &db_creditcard));
  EXPECT_EQ(target_creditcard, *db_creditcard);
  delete db_creditcard;

  // Update the 'Target' profile.
  target_creditcard.SetInfo(AutoFillType(CREDIT_CARD_NAME),
                            ASCIIToUTF16("Charles Grady"));
  EXPECT_TRUE(db.UpdateCreditCard(target_creditcard));
  ASSERT_TRUE(db.GetCreditCardForLabel(ASCIIToUTF16("Target"), &db_creditcard));
  EXPECT_EQ(target_creditcard, *db_creditcard);
  delete db_creditcard;

  // Remove the 'Billing' profile.
  EXPECT_TRUE(db.RemoveCreditCard(target_creditcard.unique_id()));
  EXPECT_FALSE(db.GetCreditCardForLabel(ASCIIToUTF16("Target"),
                                        &db_creditcard));
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
                0),
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
                0),
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
                0),
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
                  0),
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

  static int VersionFromConnection(sql::Connection* connection) {
    // Get version.
    sql::Statement s(connection->GetUniqueStatement(
        "SELECT value FROM meta WHERE key='version'"));
    if (!s.Step())
      return 0;
    return s.ColumnInt(0);
  }

  // The tables in these "Setup" routines were generated by launching the
  // Chromium application prior to schema change, then using the sqlite
  // command-line application to dump the contents of the "Web Data" database.
  // Like this:
  //   > .output foo_dump.sql
  //   > .dump
  void SetUpVersion22Database();
  void SetUpVersion22CorruptDatabase();
  void SetUpVersion24Database();
  void SetUpVersion25Database();
  void SetUpVersion26Database();

 private:
  ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseMigrationTest);
};

const int WebDatabaseMigrationTest::kCurrentTestedVersionNumber = 27;

// This schema is taken from a build prior to the addition of the |credit_card|
// table.  Version 22 of the schema.  Contrast this with the corrupt version
// below.
void WebDatabaseMigrationTest::SetUpVersion22Database() {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
    "CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
        "value LONGVARCHAR);"
    "INSERT INTO \"meta\" VALUES('version','22');"
    "INSERT INTO \"meta\" VALUES('last_compatible_version','21');"
    "INSERT INTO \"meta\" VALUES('Builtin Keyword Version','27');"
    "INSERT INTO \"meta\" VALUES('Default Search Provider ID','7');"
    "CREATE TABLE keywords (id INTEGER PRIMARY KEY,short_name VARCHAR NOT NULL,"
        "keyword VARCHAR NOT NULL,favicon_url VARCHAR NOT NULL,"
        "url VARCHAR NOT NULL,show_in_default_list INTEGER,"
        "safe_for_autoreplace INTEGER,originating_url VARCHAR,"
        "date_created INTEGER DEFAULT 0,usage_count INTEGER DEFAULT 0,"
        "input_encodings VARCHAR,suggest_url VARCHAR,"
        "prepopulate_id INTEGER DEFAULT 0,"
        "autogenerate_keyword INTEGER DEFAULT 0);"
    "INSERT INTO \"keywords\" VALUES(2,'Google','google.com',"
        "'http://www.google.com/favicon.ico',"
        "'{google:baseURL}search?{google:RLZ}{google:acceptedSuggestion}"
        "{google:originalQueryForSuggestion}sourceid=chrome&ie={inputEncoding}"
        "&q={searchTerms}',1,1,'',0,0,'UTF-8','{google:baseSuggestURL}search?"
        "client=chrome&hl={language}&q={searchTerms}',1,1);"
    "INSERT INTO \"keywords\" VALUES(3,'Yahoo!','yahoo.com',"
        "'http://search.yahoo.com/favicon.ico','http://search.yahoo.com/search?"
        "ei={inputEncoding}&fr=crmas&p={searchTerms}',1,1,'',0,0,'UTF-8',"
        "'http://ff.search.yahoo.com/gossip?output=fxjson&"
        "command={searchTerms}',2,0);"
    "INSERT INTO \"keywords\" VALUES(4,'Bing','bing.com',"
        "'http://www.bing.com/s/wlflag.ico','http://www.bing.com/search?"
        "setmkt=en-US&q={searchTerms}',1,1,'',0,0,'UTF-8',"
        "'http://api.bing.com/osjson.aspx?query={searchTerms}&"
        "language={language}',3,0);"
    "INSERT INTO \"keywords\" VALUES(5,'Wikipedia (en)','en.wikipedia.org','',"
        "'http://en.wikipedia.org/w/index.php?title=Special:Search&"
        "search={searchTerms}',1,0,'',1283287335,0,'','',0,0);"
    "INSERT INTO \"keywords\" VALUES(6,'NYTimes','query.nytimes.com','',"
        "'http://query.nytimes.com/gst/handler.html?query={searchTerms}&"
        "opensearch=1',1,0,'',1283287335,0,'','',0,0);"
    "INSERT INTO \"keywords\" VALUES(7,'eBay','rover.ebay.com','',"
        "'http://rover.ebay.com/rover/1/711-43047-14818-1/4?"
        "satitle={searchTerms}',1,0,'',1283287335,0,'','',0,0);"
    "INSERT INTO \"keywords\" VALUES(8,'ff','ff','','http://ff{searchTerms}',"
        "0,0,'',1283287356,0,'','',0,0);"
    "CREATE TABLE logins (origin_url VARCHAR NOT NULL, action_url VARCHAR, "
        "username_element VARCHAR, username_value VARCHAR, "
        "password_element VARCHAR, password_value BLOB, submit_element VARCHAR,"
        "signon_realm VARCHAR NOT NULL,ssl_valid INTEGER NOT NULL,"
        "preferred INTEGER NOT NULL,date_created INTEGER NOT NULL,"
        "blacklisted_by_user INTEGER NOT NULL,scheme INTEGER NOT NULL,"
        "UNIQUE (origin_url, username_element, username_value, "
        "password_element, submit_element, signon_realm));"
    "CREATE TABLE ie7_logins (url_hash VARCHAR NOT NULL, password_value BLOB, "
        "date_created INTEGER NOT NULL,UNIQUE (url_hash));"
    "CREATE TABLE web_app_icons (url LONGVARCHAR,width int,height int,"
        "image BLOB, UNIQUE (url, width, height));"
    "CREATE TABLE web_apps (url LONGVARCHAR UNIQUE,"
        "has_all_images INTEGER NOT NULL);"
    "CREATE TABLE autofill (name VARCHAR, value VARCHAR, value_lower VARCHAR, "
        "pair_id INTEGER PRIMARY KEY, count INTEGER DEFAULT 1);"
    "CREATE TABLE autofill_dates ( pair_id INTEGER DEFAULT 0,"
        "date_created INTEGER DEFAULT 0);"
    "CREATE INDEX logins_signon ON logins (signon_realm);"
    "CREATE INDEX ie7_logins_hash ON ie7_logins (url_hash);"
    "CREATE INDEX web_apps_url_index ON web_apps (url);"
    "CREATE INDEX autofill_name ON autofill (name);"
    "CREATE INDEX autofill_name_value_lower ON autofill (name, value_lower);"
    "CREATE INDEX autofill_dates_pair_id ON autofill_dates (pair_id);"));
  ASSERT_TRUE(connection.CommitTransaction());
}

// This schema is taken from a build after the addition of the |credit_card|
// table.  Due to a bug in the migration logic the version is set incorrectly to
// 22 (it should have been updated to 23 at least).
void WebDatabaseMigrationTest::SetUpVersion22CorruptDatabase() {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
      "CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
          "value LONGVARCHAR);"
      "INSERT INTO \"meta\" VALUES('version','22');"
      "INSERT INTO \"meta\" VALUES('last_compatible_version','21');"
      "INSERT INTO \"meta\" VALUES('Default Search Provider ID','2');"
      "INSERT INTO \"meta\" VALUES('Builtin Keyword Version','29');"
      "CREATE TABLE keywords (id INTEGER PRIMARY KEY,"
          "short_name VARCHAR NOT NULL,keyword VARCHAR NOT NULL,"
          "favicon_url VARCHAR NOT NULL,url VARCHAR NOT NULL,"
          "show_in_default_list INTEGER,safe_for_autoreplace INTEGER,"
          "originating_url VARCHAR,date_created INTEGER DEFAULT 0,"
          "usage_count INTEGER DEFAULT 0,input_encodings VARCHAR,"
          "suggest_url VARCHAR,prepopulate_id INTEGER DEFAULT 0,"
          "autogenerate_keyword INTEGER DEFAULT 0);"
      "INSERT INTO \"keywords\" VALUES(2,'Google','google.com',"
          "'http://www.google.com/favicon.ico','{google:baseURL}search?"
          "{google:RLZ}{google:acceptedSuggestion}"
          "{google:originalQueryForSuggestion}sourceid=chrome&"
          "ie={inputEncoding}&q={searchTerms}',1,1,'',0,0,'UTF-8',"
          "'{google:baseSuggestURL}search?client=chrome&hl={language}&"
          "q={searchTerms}',1,1);"
      "INSERT INTO \"keywords\" VALUES(3,'Yahoo!','yahoo.com',"
          "'http://search.yahoo.com/favicon.ico',"
          "'http://search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
          "p={searchTerms}',1,1,'',0,0,'UTF-8',"
          "'http://ff.search.yahoo.com/gossip?output=fxjson&"
          "command={searchTerms}',2,0);"
      "INSERT INTO \"keywords\" VALUES(4,'Bing','bing.com',"
          "'http://www.bing.com/s/wlflag.ico','http://www.bing.com/search?"
          "setmkt=en-US&q={searchTerms}',1,1,'',0,0,'UTF-8',"
          "'http://api.bing.com/osjson.aspx?query={searchTerms}&"
          "language={language}',3,0);"
      "INSERT INTO \"keywords\" VALUES(5,'Wikipedia (en)','en.wikipedia.org',"
          "'','http://en.wikipedia.org/w/index.php?title=Special:Search&"
          "search={searchTerms}',1,0,'',1283287335,0,'','',0,0);"
      "INSERT INTO \"keywords\" VALUES(6,'NYTimes','query.nytimes.com','',"
          "'http://query.nytimes.com/gst/handler.html?query={searchTerms}&"
          "opensearch=1',1,0,'',1283287335,0,'','',0,0);"
      "INSERT INTO \"keywords\" VALUES(7,'eBay','rover.ebay.com','',"
          "'http://rover.ebay.com/rover/1/711-43047-14818-1/4?"
          "satitle={searchTerms}',1,0,'',1283287335,0,'','',0,0);"
      "INSERT INTO \"keywords\" VALUES(8,'ff','ff','','http://ff{searchTerms}'"
          ",0,0,'',1283287356,0,'','',0,0);"
      "CREATE TABLE logins (origin_url VARCHAR NOT NULL, action_url VARCHAR, "
          "username_element VARCHAR, username_value VARCHAR, "
          "password_element VARCHAR, password_value BLOB, "
          "submit_element VARCHAR, signon_realm VARCHAR NOT NULL,"
          "ssl_valid INTEGER NOT NULL,preferred INTEGER NOT NULL,"
          "date_created INTEGER NOT NULL,blacklisted_by_user INTEGER NOT NULL,"
          "scheme INTEGER NOT NULL,UNIQUE (origin_url, username_element, "
          "username_value, password_element, submit_element, signon_realm));"
      "CREATE TABLE ie7_logins (url_hash VARCHAR NOT NULL, password_value BLOB,"
          "date_created INTEGER NOT NULL,UNIQUE (url_hash));"
      "CREATE TABLE web_app_icons (url LONGVARCHAR,width int,height int,"
          "image BLOB, UNIQUE (url, width, height));"
      "CREATE TABLE web_apps (url LONGVARCHAR UNIQUE,"
          "has_all_images INTEGER NOT NULL);"
      "CREATE TABLE autofill (name VARCHAR, value VARCHAR, value_lower VARCHAR,"
          "pair_id INTEGER PRIMARY KEY, count INTEGER DEFAULT 1);"
      "CREATE TABLE autofill_dates ( pair_id INTEGER DEFAULT 0,"
          "date_created INTEGER DEFAULT 0);"
      "CREATE TABLE autofill_profiles ( label VARCHAR, "
          "unique_id INTEGER PRIMARY KEY, first_name VARCHAR, "
          "middle_name VARCHAR, last_name VARCHAR, email VARCHAR, "
          "company_name VARCHAR, address_line_1 VARCHAR, "
          "address_line_2 VARCHAR, city VARCHAR, state VARCHAR, "
          "zipcode VARCHAR, country VARCHAR, phone VARCHAR, fax VARCHAR);"
      "CREATE TABLE credit_cards ( label VARCHAR, "
          "unique_id INTEGER PRIMARY KEY, name_on_card VARCHAR, type VARCHAR,"
          "card_number VARCHAR, expiration_month INTEGER, "
          "expiration_year INTEGER, verification_code VARCHAR, "
          "billing_address VARCHAR, shipping_address VARCHAR, "
          "card_number_encrypted BLOB, verification_code_encrypted BLOB);"
      "CREATE INDEX logins_signon ON logins (signon_realm);"
      "CREATE INDEX ie7_logins_hash ON ie7_logins (url_hash);"
      "CREATE INDEX web_apps_url_index ON web_apps (url);"
      "CREATE INDEX autofill_name ON autofill (name);"
      "CREATE INDEX autofill_name_value_lower ON autofill (name, value_lower);"
      "CREATE INDEX autofill_dates_pair_id ON autofill_dates (pair_id);"
      "CREATE INDEX autofill_profiles_label_index ON autofill_profiles (label);"
      "CREATE INDEX credit_cards_label_index ON credit_cards (label);"));
  ASSERT_TRUE(connection.CommitTransaction());
}

// This schema is taken from a build prior to the addition of the |keywords|
// |logo_id| column.
void WebDatabaseMigrationTest::SetUpVersion24Database() {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
    "CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
        "value LONGVARCHAR);"
    "INSERT INTO \"meta\" VALUES('version','24');"
    "INSERT INTO \"meta\" VALUES('last_compatible_version','24');"
    "CREATE TABLE keywords (id INTEGER PRIMARY KEY,short_name VARCHAR NOT NULL,"
        "keyword VARCHAR NOT NULL,favicon_url VARCHAR NOT NULL,"
        "url VARCHAR NOT NULL,show_in_default_list INTEGER,"
        "safe_for_autoreplace INTEGER,originating_url VARCHAR,"
        "date_created INTEGER DEFAULT 0,usage_count INTEGER DEFAULT 0,"
        "input_encodings VARCHAR,suggest_url VARCHAR,"
        "prepopulate_id INTEGER DEFAULT 0,"
        "autogenerate_keyword INTEGER DEFAULT 0);"
    "CREATE TABLE logins (origin_url VARCHAR NOT NULL, action_url VARCHAR,"
        "username_element VARCHAR, username_value VARCHAR,"
        "password_element VARCHAR, password_value BLOB, submit_element VARCHAR,"
        "signon_realm VARCHAR NOT NULL,"
        "ssl_valid INTEGER NOT NULL,preferred INTEGER NOT NULL,"
        "date_created INTEGER NOT NULL,blacklisted_by_user INTEGER NOT NULL,"
        "scheme INTEGER NOT NULL,UNIQUE (origin_url, username_element,"
        "username_value, password_element, submit_element, signon_realm));"
    "CREATE TABLE web_app_icons (url LONGVARCHAR,width int,height int,"
        "image BLOB, UNIQUE (url, width, height));"
    "CREATE TABLE web_apps (url LONGVARCHAR UNIQUE,"
        "has_all_images INTEGER NOT NULL);"
    "CREATE TABLE autofill (name VARCHAR, value VARCHAR, value_lower VARCHAR,"
        "pair_id INTEGER PRIMARY KEY, count INTEGER DEFAULT 1);"
    "CREATE TABLE autofill_dates ( pair_id INTEGER DEFAULT 0,"
        "date_created INTEGER DEFAULT 0);"
    "CREATE TABLE autofill_profiles ( label VARCHAR,"
        "unique_id INTEGER PRIMARY KEY, first_name VARCHAR,"
        "middle_name VARCHAR, last_name VARCHAR, email VARCHAR,"
        "company_name VARCHAR, address_line_1 VARCHAR, address_line_2 VARCHAR,"
        "city VARCHAR, state VARCHAR, zipcode VARCHAR, country VARCHAR,"
        "phone VARCHAR, fax VARCHAR);"
    "CREATE TABLE credit_cards ( label VARCHAR, unique_id INTEGER PRIMARY KEY,"
        "name_on_card VARCHAR, type VARCHAR, card_number VARCHAR,"
        "expiration_month INTEGER, expiration_year INTEGER,"
        "verification_code VARCHAR, billing_address VARCHAR,"
        "shipping_address VARCHAR, card_number_encrypted BLOB,"
        "verification_code_encrypted BLOB);"
    "CREATE TABLE token_service (service VARCHAR PRIMARY KEY NOT NULL,"
        "encrypted_token BLOB);"
    "CREATE INDEX logins_signon ON logins (signon_realm);"
    "CREATE INDEX web_apps_url_index ON web_apps (url);"
    "CREATE INDEX autofill_name ON autofill (name);"
    "CREATE INDEX autofill_name_value_lower ON autofill (name, value_lower);"
    "CREATE INDEX autofill_dates_pair_id ON autofill_dates (pair_id);"
    "CREATE INDEX autofill_profiles_label_index ON autofill_profiles (label);"
    "CREATE INDEX credit_cards_label_index ON credit_cards (label);"));
  ASSERT_TRUE(connection.CommitTransaction());
}

// This schema is taken from a build prior to the addition of the |keywords|
// |created_by_policy| column.
void WebDatabaseMigrationTest::SetUpVersion25Database() {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
    "CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
        "value LONGVARCHAR);"
    "INSERT INTO \"meta\" VALUES('version','25');"
    "INSERT INTO \"meta\" VALUES('last_compatible_version','25');"
    "CREATE TABLE keywords (id INTEGER PRIMARY KEY,short_name VARCHAR NOT NULL,"
        "keyword VARCHAR NOT NULL,favicon_url VARCHAR NOT NULL,"
        "url VARCHAR NOT NULL,show_in_default_list INTEGER,"
        "safe_for_autoreplace INTEGER,originating_url VARCHAR,"
        "date_created INTEGER DEFAULT 0,usage_count INTEGER DEFAULT 0,"
        "input_encodings VARCHAR,suggest_url VARCHAR,"
        "prepopulate_id INTEGER DEFAULT 0,"
        "autogenerate_keyword INTEGER DEFAULT 0,"
        "logo_id INTEGER DEFAULT 0);"
    "CREATE TABLE logins (origin_url VARCHAR NOT NULL, action_url VARCHAR,"
        "username_element VARCHAR, username_value VARCHAR,"
        "password_element VARCHAR, password_value BLOB, submit_element VARCHAR,"
        "signon_realm VARCHAR NOT NULL,"
        "ssl_valid INTEGER NOT NULL,preferred INTEGER NOT NULL,"
        "date_created INTEGER NOT NULL,blacklisted_by_user INTEGER NOT NULL,"
        "scheme INTEGER NOT NULL,UNIQUE (origin_url, username_element,"
        "username_value, password_element, submit_element, signon_realm));"
    "CREATE TABLE web_app_icons (url LONGVARCHAR,width int,height int,"
        "image BLOB, UNIQUE (url, width, height));"
    "CREATE TABLE web_apps (url LONGVARCHAR UNIQUE,"
        "has_all_images INTEGER NOT NULL);"
    "CREATE TABLE autofill (name VARCHAR, value VARCHAR, value_lower VARCHAR,"
        "pair_id INTEGER PRIMARY KEY, count INTEGER DEFAULT 1);"
    "CREATE TABLE autofill_dates ( pair_id INTEGER DEFAULT 0,"
        "date_created INTEGER DEFAULT 0);"
    "CREATE TABLE autofill_profiles ( label VARCHAR,"
        "unique_id INTEGER PRIMARY KEY, first_name VARCHAR,"
        "middle_name VARCHAR, last_name VARCHAR, email VARCHAR,"
        "company_name VARCHAR, address_line_1 VARCHAR, address_line_2 VARCHAR,"
        "city VARCHAR, state VARCHAR, zipcode VARCHAR, country VARCHAR,"
        "phone VARCHAR, fax VARCHAR);"
    "CREATE TABLE credit_cards ( label VARCHAR, unique_id INTEGER PRIMARY KEY,"
        "name_on_card VARCHAR, type VARCHAR, card_number VARCHAR,"
        "expiration_month INTEGER, expiration_year INTEGER,"
        "verification_code VARCHAR, billing_address VARCHAR,"
        "shipping_address VARCHAR, card_number_encrypted BLOB,"
        "verification_code_encrypted BLOB);"
    "CREATE TABLE token_service (service VARCHAR PRIMARY KEY NOT NULL,"
        "encrypted_token BLOB);"
    "CREATE INDEX logins_signon ON logins (signon_realm);"
    "CREATE INDEX web_apps_url_index ON web_apps (url);"
    "CREATE INDEX autofill_name ON autofill (name);"
    "CREATE INDEX autofill_name_value_lower ON autofill (name, value_lower);"
    "CREATE INDEX autofill_dates_pair_id ON autofill_dates (pair_id);"
    "CREATE INDEX autofill_profiles_label_index ON autofill_profiles (label);"
    "CREATE INDEX credit_cards_label_index ON credit_cards (label);"));
  ASSERT_TRUE(connection.CommitTransaction());
}

// This schema is taken from a build prior to the change of column type for
// credit_cards.billing_address from string to int.
void WebDatabaseMigrationTest::SetUpVersion26Database() {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
    "CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
        "value LONGVARCHAR);"
    "INSERT INTO \"meta\" VALUES('version','25');"
    "INSERT INTO \"meta\" VALUES('last_compatible_version','25');"
    "CREATE TABLE keywords (id INTEGER PRIMARY KEY,short_name VARCHAR NOT NULL,"
        "keyword VARCHAR NOT NULL,favicon_url VARCHAR NOT NULL,"
        "url VARCHAR NOT NULL,show_in_default_list INTEGER,"
        "safe_for_autoreplace INTEGER,originating_url VARCHAR,"
        "date_created INTEGER DEFAULT 0,usage_count INTEGER DEFAULT 0,"
        "input_encodings VARCHAR,suggest_url VARCHAR,"
        "prepopulate_id INTEGER DEFAULT 0,"
        "autogenerate_keyword INTEGER DEFAULT 0,"
        "logo_id INTEGER DEFAULT 0,"
        "created_by_policy INTEGER DEFAULT 0);"
    "CREATE TABLE logins (origin_url VARCHAR NOT NULL, action_url VARCHAR,"
        "username_element VARCHAR, username_value VARCHAR,"
        "password_element VARCHAR, password_value BLOB, submit_element VARCHAR,"
        "signon_realm VARCHAR NOT NULL,"
        "ssl_valid INTEGER NOT NULL,preferred INTEGER NOT NULL,"
        "date_created INTEGER NOT NULL,blacklisted_by_user INTEGER NOT NULL,"
        "scheme INTEGER NOT NULL,UNIQUE (origin_url, username_element,"
        "username_value, password_element, submit_element, signon_realm));"
    "CREATE TABLE web_app_icons (url LONGVARCHAR,width int,height int,"
        "image BLOB, UNIQUE (url, width, height));"
    "CREATE TABLE web_apps (url LONGVARCHAR UNIQUE,"
        "has_all_images INTEGER NOT NULL);"
    "CREATE TABLE autofill (name VARCHAR, value VARCHAR, value_lower VARCHAR,"
        "pair_id INTEGER PRIMARY KEY, count INTEGER DEFAULT 1);"
    "CREATE TABLE autofill_dates ( pair_id INTEGER DEFAULT 0,"
        "date_created INTEGER DEFAULT 0);"
    "CREATE TABLE autofill_profiles ( label VARCHAR,"
        "unique_id INTEGER PRIMARY KEY, first_name VARCHAR,"
        "middle_name VARCHAR, last_name VARCHAR, email VARCHAR,"
        "company_name VARCHAR, address_line_1 VARCHAR, address_line_2 VARCHAR,"
        "city VARCHAR, state VARCHAR, zipcode VARCHAR, country VARCHAR,"
        "phone VARCHAR, fax VARCHAR);"
    "CREATE TABLE credit_cards ( label VARCHAR, unique_id INTEGER PRIMARY KEY,"
        "name_on_card VARCHAR, type VARCHAR, card_number VARCHAR,"
        "expiration_month INTEGER, expiration_year INTEGER,"
        "verification_code VARCHAR, billing_address VARCHAR,"
        "shipping_address VARCHAR, card_number_encrypted BLOB,"
        "verification_code_encrypted BLOB);"
    "CREATE TABLE token_service (service VARCHAR PRIMARY KEY NOT NULL,"
        "encrypted_token BLOB);"
    "CREATE INDEX logins_signon ON logins (signon_realm);"
    "CREATE INDEX web_apps_url_index ON web_apps (url);"
    "CREATE INDEX autofill_name ON autofill (name);"
    "CREATE INDEX autofill_name_value_lower ON autofill (name, value_lower);"
    "CREATE INDEX autofill_dates_pair_id ON autofill_dates (pair_id);"
    "CREATE INDEX autofill_profiles_label_index ON autofill_profiles (label);"
    "CREATE INDEX credit_cards_label_index ON credit_cards (label);"));
  ASSERT_TRUE(connection.CommitTransaction());
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
  // Initialize the database.
  SetUpVersion22Database();

  // Verify pre-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // No |credit_card| table prior to version 23.
    ASSERT_FALSE(connection.DoesColumnExist("credit_cards", "unique_id"));
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
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "unique_id"));
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
  // Initialize the database.
  SetUpVersion22CorruptDatabase();

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
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "unique_id"));
    EXPECT_TRUE(
        connection.DoesColumnExist("credit_cards", "card_number_encrypted"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "id"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "logo_id"));
  }
}

// Tests that the |keywords| |logo_id| column gets added to the schema for a
// version 24 database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion24ToCurrent) {
  // Initialize the database.
  SetUpVersion24Database();

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
  // Initialize the database.
  SetUpVersion25Database();

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
  // Initialize the database.
  SetUpVersion25Database();

  // Verify pre-conditions. These are expectations for version 25 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns existing and not existing before current version.
    ASSERT_TRUE(connection.DoesColumnExist("keywords", "id"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords", "created_by_policy"));
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
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "billing_address"));

    // |billing_address| is an integer. Also Verify the credit card data is
    // converted.
    std::string stmt = "SELECT * FROM credit_cards";
    sql::Statement s(connection.GetUniqueStatement(stmt.c_str()));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnType(8), sql::COLUMN_TYPE_INTEGER);
    EXPECT_EQ("label", s.ColumnString(0));
    EXPECT_EQ(2, s.ColumnInt(1));
    EXPECT_EQ("Jack", s.ColumnString(2));
    EXPECT_EQ("Visa", s.ColumnString(3));
    EXPECT_EQ("1234", s.ColumnString(4));
    EXPECT_EQ(2, s.ColumnInt(5));
    EXPECT_EQ(2012, s.ColumnInt(6));
    EXPECT_EQ(std::string(), s.ColumnString(7));
    EXPECT_EQ(1, s.ColumnInt(8));
    // The remaining columns are unused or blobs.
  }
}

// Tests that the credit_cards.billing_address column is changed from a string
// to an int whilst preserving the associated billing address. This version of
// the test makes sure a stored string ID is converted to an integer ID.
TEST_F(WebDatabaseMigrationTest, MigrateVersion26ToCurrentStringIDs) {
  // Initialize the database.
  SetUpVersion25Database();

  // Verify pre-conditions. These are expectations for version 25 of the
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
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "billing_address"));

    // |billing_address| is an integer. Also Verify the credit card data is
    // converted.
    std::string stmt = "SELECT * FROM credit_cards";
    sql::Statement s(connection.GetUniqueStatement(stmt.c_str()));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnType(8), sql::COLUMN_TYPE_INTEGER);
    EXPECT_EQ("label", s.ColumnString(0));
    EXPECT_EQ(2, s.ColumnInt(1));
    EXPECT_EQ("Jack", s.ColumnString(2));
    EXPECT_EQ("Visa", s.ColumnString(3));
    EXPECT_EQ("1234", s.ColumnString(4));
    EXPECT_EQ(2, s.ColumnInt(5));
    EXPECT_EQ(2012, s.ColumnInt(6));
    EXPECT_EQ(std::string(), s.ColumnString(7));
    EXPECT_EQ(1, s.ColumnInt(8));
    // The remaining columns are unused or blobs.
  }
}
