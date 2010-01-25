// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/search_engines/template_url.h"
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

class WebDatabaseTest : public testing::Test {
 protected:
  typedef std::vector<AutofillChange> AutofillChangeList;
  virtual void SetUp() {
    PathService::Get(chrome::DIR_TEST_DATA, &file_);
    const std::string test_db = "TestWebDatabase" +
        Int64ToString(base::Time::Now().ToInternalValue()) +
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
    std::wstring av, bv;
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

  FilePath file_;
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
  template_url.SetURL(L"http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  template_url.set_show_in_default_list(true);
  template_url.set_originating_url(originating_url);
  template_url.set_usage_count(32);
  template_url.add_input_encoding("UTF-8");
  set_prepopulate_id(&template_url, 10);
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

  EXPECT_TRUE(restored_url->show_in_default_list());

  EXPECT_EQ(GetID(&template_url), GetID(restored_url));

  EXPECT_TRUE(originating_url == restored_url->originating_url());

  EXPECT_EQ(32, restored_url->usage_count());

  ASSERT_EQ(1U, restored_url->input_encodings().size());
  EXPECT_EQ("UTF-8", restored_url->input_encodings()[0]);

  EXPECT_EQ(10, restored_url->prepopulate_id());

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
  template_url.SetURL(L"http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  template_url.set_show_in_default_list(true);
  template_url.SetSuggestionsURL(L"url2", 0, 0);
  SetID(1, &template_url);

  EXPECT_TRUE(db.AddKeyword(template_url));

  GURL originating_url2("http://originating.url/");
  template_url.set_originating_url(originating_url2);
  template_url.set_autogenerate_keyword(true);
  EXPECT_EQ(L"url", template_url.keyword());
  template_url.add_input_encoding("Shift_JIS");
  set_prepopulate_id(&template_url, 5);
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

  delete restored_url;
}

TEST_F(WebDatabaseTest, KeywordWithNoFavicon) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  TemplateURL template_url;
  template_url.set_short_name(L"short_name");
  template_url.set_keyword(L"keyword");
  template_url.SetURL(L"http://url/", 0, 0);
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
                string16(),
                ASCIIToUTF16("Superman")),
      &changes));
  std::vector<string16> v;
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(db.AddFormFieldValue(
        FormField(string16(),
                  ASCIIToUTF16("Name"),
                  string16(),
                  ASCIIToUTF16("Clark Kent")),
        &changes));
  }
  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(db.AddFormFieldValue(
        FormField(string16(),
                  ASCIIToUTF16("Name"),
                  string16(),
                  ASCIIToUTF16("Clark Sutter")),
        &changes));
  }
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(db.AddFormFieldValue(
        FormField(string16(),
                  ASCIIToUTF16("Favorite Color"),
                  string16(),
                  ASCIIToUTF16("Green")),
        &changes));
  }

  int count = 0;
  int64 pair_id = 0;

  // We have added the name Clark Kent 5 times, so count should be 5 and pair_id
  // should be somthing non-zero.
  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                string16(),
                ASCIIToUTF16("Clark Kent")),
      &pair_id, &count));
  EXPECT_EQ(5, count);
  EXPECT_NE(0, pair_id);

  // Storing in the data base should be case sensitive, so there should be no
  // database entry for clark kent lowercase.
  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                string16(),
                ASCIIToUTF16("clark kent")),
      &pair_id, &count));
  EXPECT_EQ(0, count);

  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      FormField(string16(),
                ASCIIToUTF16("Favorite Color"),
                string16(),
                ASCIIToUTF16("Green")),
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
                string16(),
                ASCIIToUTF16("Clark Kent")),
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
                                             string16()),
                                   &changes));
  EXPECT_TRUE(db.AddFormFieldValue(FormField(string16(),
                                             ASCIIToUTF16("blank"),
                                             string16(),
                                             ASCIIToUTF16(" ")),
                                   &changes));
  EXPECT_TRUE(db.AddFormFieldValue(FormField(string16(),
                                             ASCIIToUTF16("blank"),
                                             string16(),
                                             ASCIIToUTF16("      ")),
                                   &changes));
  EXPECT_TRUE(db.AddFormFieldValue(FormField(string16(),
                                             ASCIIToUTF16("blank"),
                                             string16(),
                                             kValue),
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
                string16(),
                ASCIIToUTF16("Superman")),
      &changes,
      t1));
  EXPECT_TRUE(db.AddFormFieldValueTime(
      FormField(string16(),
                ASCIIToUTF16("Name"),
                string16(),
                ASCIIToUTF16("Superman")),
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
                string16(),
                ASCIIToUTF16("Superman")),
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
                string16(),
                ASCIIToUTF16("Superman")),
      &changes,
      t2));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::UPDATE,
                             AutofillKey(ASCIIToUTF16("Name"),
                                         ASCIIToUTF16("Superman"))),
            changes[0]);
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
  home_profile.SetInfo(AutoFillType(PHONE_HOME_NUMBER),
                       ASCIIToUTF16("18181234567"));
  home_profile.SetInfo(AutoFillType(PHONE_FAX_NUMBER),
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
  work_creditcard.SetInfo(AutoFillType(CREDIT_CARD_VERIFICATION_CODE),
                          ASCIIToUTF16("987"));
  work_creditcard.set_billing_address(ASCIIToUTF16("Overlook Hotel"));
  work_creditcard.set_shipping_address(ASCIIToUTF16("Timberline Lodge"));

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
  target_creditcard.SetInfo(AutoFillType(CREDIT_CARD_VERIFICATION_CODE),
                            ASCIIToUTF16("123"));
  target_creditcard.set_billing_address(ASCIIToUTF16("Overlook Hotel"));
  target_creditcard.set_shipping_address(string16());

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
