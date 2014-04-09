// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using ::testing::Eq;

class LoginDatabaseTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.path().AppendASCII("TestMetadataStoreMacDatabase");

    ASSERT_TRUE(db_.Init(file_));
  }

  Pickle SerializeVector(const std::vector<base::string16>& vec) const {
    return db_.SerializeVector(vec);
  }

  std::vector<base::string16> DeserializeVector(const Pickle& pickle) const {
    return db_.DeserializeVector(pickle);
  }

  void FormsAreEqual(const PasswordForm& expected, const PasswordForm& actual) {
    PasswordForm expected_copy(expected);
#if defined(OS_MACOSX) && !defined(OS_IOS)
    // On the Mac we should never be storing passwords in the database.
    expected_copy.password_value = ASCIIToUTF16("");
#endif
    EXPECT_EQ(expected_copy, actual);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_;
  LoginDatabase db_;
};

TEST_F(LoginDatabaseTest, Logins) {
  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.action = GURL("http://accounts.google.com/Login");
  form.username_element = ASCIIToUTF16("Email");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("Passwd");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = "http://www.google.com/";
  form.ssl_valid = false;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;
  form.times_used = 1;
  form.form_data.name = ASCIIToUTF16("form_name");
  form.form_data.method = ASCIIToUTF16("POST");

  // Add it and make sure it is there and that all the fields were retrieved
  // correctly.
  EXPECT_TRUE(db_.AddLogin(form));
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  FormsAreEqual(form, *result[0]);
  delete result[0];
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db_.GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The example site changes...
  PasswordForm form2(form);
  form2.origin = GURL("http://www.google.com/new/accounts/LoginAuth");
  form2.submit_element = ASCIIToUTF16("reallySignIn");

  // Match against an inexact copy
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Uh oh, the site changed origin & action URLs all at once!
  PasswordForm form3(form2);
  form3.action = GURL("http://www.google.com/new/accounts/Login");

  // signon_realm is the same, should match.
  EXPECT_TRUE(db_.GetLogins(form3, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Imagine the site moves to a secure server for login.
  PasswordForm form4(form3);
  form4.signon_realm = "https://www.google.com/";
  form4.ssl_valid = true;

  // We have only an http record, so no match for this.
  EXPECT_TRUE(db_.GetLogins(form4, &result));
  EXPECT_EQ(0U, result.size());

  // Let's imagine the user logs into the secure site.
  EXPECT_TRUE(db_.AddLogin(form4));
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  delete result[0];
  delete result[1];
  result.clear();

  // Now the match works
  EXPECT_TRUE(db_.GetLogins(form4, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The user chose to forget the original but not the new.
  EXPECT_TRUE(db_.RemoveLogin(form));
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The old form wont match the new site (http vs https).
  EXPECT_TRUE(db_.GetLogins(form, &result));
  EXPECT_EQ(0U, result.size());

  // The user's request for the HTTPS site is intercepted
  // by an attacker who presents an invalid SSL cert.
  PasswordForm form5(form4);
  form5.ssl_valid = 0;

  // It will match in this case.
  EXPECT_TRUE(db_.GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // User changes his password.
  PasswordForm form6(form5);
  form6.password_value = ASCIIToUTF16("test6");
  form6.preferred = true;

  // We update, and check to make sure it matches the
  // old form, and there is only one record.
  int rows_changed = 0;
  EXPECT_TRUE(db_.UpdateLogin(form6, &rows_changed));
  EXPECT_EQ(1, rows_changed);
  // matches
  EXPECT_TRUE(db_.GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();
  // Only one record.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  // Password element was updated.
#if defined(OS_MACOSX) && !defined(OS_IOS)
  // On the Mac we should never be storing passwords in the database.
  EXPECT_EQ(base::string16(), result[0]->password_value);
#else
  EXPECT_EQ(form6.password_value, result[0]->password_value);
#endif
  // Preferred login.
  EXPECT_TRUE(form6.preferred);
  delete result[0];
  result.clear();

  // Make sure everything can disappear.
  EXPECT_TRUE(db_.RemoveLogin(form4));
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatching) {
  PSLMatchingHelper::EnablePublicSuffixDomainMatchingForTesting();
  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://foo.com/";
  form.ssl_valid = true;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_TRUE(db_.AddLogin(form));
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db_.GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // We go to the mobile site.
  PasswordForm form2(form);
  form2.origin = GURL("https://mobile.foo.com/");
  form2.action = GURL("https://mobile.foo.com/login");
  form2.signon_realm = "https://mobile.foo.com/";

  // Match against the mobile site.
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://mobile.foo.com/", result[0]->signon_realm);
  EXPECT_EQ("https://foo.com/", result[0]->original_signon_realm);
  delete result[0];
  result.clear();
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingShouldMatchingApply) {
  PSLMatchingHelper::EnablePublicSuffixDomainMatchingForTesting();
  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://accounts.google.com/");
  form.action = GURL("https://accounts.google.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://accounts.google.com/";
  form.ssl_valid = true;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_TRUE(db_.AddLogin(form));
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db_.GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // We go to a different site on the same domain where feature is not needed.
  PasswordForm form2(form);
  form2.origin = GURL("https://some.other.google.com/");
  form2.action = GURL("https://some.other.google.com/login");
  form2.signon_realm = "https://some.other.google.com/";

  // Match against the other site. Should not match since feature should not be
  // enabled for this domain.
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());
}

// This test fails if the implementation of GetLogins uses GetCachedStatement
// instead of GetUniqueStatement, since REGEXP is in use. See
// http://crbug.com/248608.
TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingDifferentSites) {
  PSLMatchingHelper::EnablePublicSuffixDomainMatchingForTesting();
  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://foo.com/";
  form.ssl_valid = true;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_TRUE(db_.AddLogin(form));
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db_.GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // We go to the mobile site.
  PasswordForm form2(form);
  form2.origin = GURL("https://mobile.foo.com/");
  form2.action = GURL("https://mobile.foo.com/login");
  form2.signon_realm = "https://mobile.foo.com/";

  // Match against the mobile site.
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://mobile.foo.com/", result[0]->signon_realm);
  EXPECT_EQ("https://foo.com/", result[0]->original_signon_realm);
  delete result[0];
  result.clear();

  // Add baz.com desktop site.
  form.origin = GURL("https://baz.com/login/");
  form.action = GURL("https://baz.com/login/");
  form.username_element = ASCIIToUTF16("email");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://baz.com/";
  form.ssl_valid = true;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_TRUE(db_.AddLogin(form));
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  delete result[0];
  delete result[1];
  result.clear();

  // We go to the mobile site of baz.com.
  PasswordForm form3(form);
  form3.origin = GURL("https://m.baz.com/login/");
  form3.action = GURL("https://m.baz.com/login/");
  form3.signon_realm = "https://m.baz.com/";

  // Match against the mobile site of baz.com.
  EXPECT_TRUE(db_.GetLogins(form3, &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://m.baz.com/", result[0]->signon_realm);
  EXPECT_EQ("https://baz.com/", result[0]->original_signon_realm);
  delete result[0];
  result.clear();
}

PasswordForm GetFormWithNewSignonRealm(PasswordForm form,
                                       std::string signon_realm) {
  PasswordForm form2(form);
  form2.origin = GURL(signon_realm);
  form2.action = GURL(signon_realm);
  form2.signon_realm = signon_realm;
  return form2;
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingRegexp) {
  PSLMatchingHelper::EnablePublicSuffixDomainMatchingForTesting();
  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("http://foo.com/");
  form.action = GURL("http://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "http://foo.com/";
  form.ssl_valid = false;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_TRUE(db_.AddLogin(form));
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Example password form that has - in the domain name.
  PasswordForm form_dash =
      GetFormWithNewSignonRealm(form, "http://www.foo-bar.com/");

  // Add it and make sure it is there.
  EXPECT_TRUE(db_.AddLogin(form_dash));
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  delete result[0];
  delete result[1];
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db_.GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // www.foo.com should match.
  PasswordForm form2 = GetFormWithNewSignonRealm(form, "http://www.foo.com/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // a.b.foo.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a.b.foo.com/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // a-b.foo.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a-b.foo.com/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo-bar.com/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // www.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://www.foo-bar.com/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // a.b.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a.b.foo-bar.com/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // a-b.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a-b.foo-bar.com/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // foo.com with port 1337 should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo.com:1337/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());

  // http://foo.com should not match since the scheme is wrong.
  form2 = GetFormWithNewSignonRealm(form, "https://foo.com/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());

  // notfoo.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://notfoo.com/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());

  // baz.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://baz.com/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());

  // foo-baz.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo-baz.com/");
  EXPECT_TRUE(db_.GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());
}

static bool AddTimestampedLogin(LoginDatabase* db, std::string url,
                                const std::string& unique_string,
                                const base::Time& time) {
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

TEST_F(LoginDatabaseTest, ClearPrivateData_SavedPasswords) {
  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  base::Time now = base::Time::Now();
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

  // Create one with a 0 time.
  EXPECT_TRUE(AddTimestampedLogin(&db_, "1", "foo1", base::Time()));
  // Create one for now and +/- 1 day.
  EXPECT_TRUE(AddTimestampedLogin(&db_, "2", "foo2", now - one_day));
  EXPECT_TRUE(AddTimestampedLogin(&db_, "3", "foo3", now));
  EXPECT_TRUE(AddTimestampedLogin(&db_, "4", "foo4", now + one_day));

  // Verify inserts worked.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(4U, result.size());
  ClearResults(&result);

  // Get everything from today's date and on.
  EXPECT_TRUE(db_.GetLoginsCreatedBetween(now, base::Time(), &result));
  EXPECT_EQ(2U, result.size());
  ClearResults(&result);

  // Delete everything from today's date and on.
  db_.RemoveLoginsCreatedBetween(now, base::Time());

  // Should have deleted half of what we inserted.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  ClearResults(&result);

  // Delete with 0 date (should delete all).
  db_.RemoveLoginsCreatedBetween(base::Time(), base::Time());

  // Verify nothing is left.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, BlacklistedLogins) {
  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db_.GetBlacklistLogins(&result));
  ASSERT_EQ(0U, result.size());

  // Save a form as blacklisted.
  PasswordForm form;
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.action = GURL("http://accounts.google.com/Login");
  form.username_element = ASCIIToUTF16("Email");
  form.password_element = ASCIIToUTF16("Passwd");
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = "http://www.google.com/";
  form.ssl_valid = false;
  form.preferred = true;
  form.blacklisted_by_user = true;
  form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_TRUE(db_.AddLogin(form));

  // Get all non-blacklisted logins (should be none).
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  ASSERT_EQ(0U, result.size());

  // GetLogins should give the blacklisted result.
  EXPECT_TRUE(db_.GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  ClearResults(&result);

  // So should GetAllBlacklistedLogins.
  EXPECT_TRUE(db_.GetBlacklistLogins(&result));
  EXPECT_EQ(1U, result.size());
  ClearResults(&result);
}

TEST_F(LoginDatabaseTest, VectorSerialization) {
  // Empty vector.
  std::vector<base::string16> vec;
  Pickle temp = SerializeVector(vec);
  std::vector<base::string16> output = DeserializeVector(temp);
  EXPECT_THAT(output, Eq(vec));

  // Normal data.
  vec.push_back(ASCIIToUTF16("first"));
  vec.push_back(ASCIIToUTF16("second"));
  vec.push_back(ASCIIToUTF16("third"));

  temp = SerializeVector(vec);
  output = DeserializeVector(temp);
  EXPECT_THAT(output, Eq(vec));
}

TEST_F(LoginDatabaseTest, UpdateIncompleteCredentials) {
  std::vector<autofill::PasswordForm*> result;
  // Verify the database is empty.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result));
  ASSERT_EQ(0U, result.size());

  // Save an incomplete form. Note that it only has a few fields set, ex. it's
  // missing 'action', 'username_element' and 'password_element'. Such forms
  // are sometimes inserted during import from other browsers (which may not
  // store this info).
  PasswordForm incomplete_form;
  incomplete_form.origin = GURL("http://accounts.google.com/LoginAuth");
  incomplete_form.signon_realm = "http://accounts.google.com/";
  incomplete_form.username_value = ASCIIToUTF16("my_username");
  incomplete_form.password_value = ASCIIToUTF16("my_password");
  incomplete_form.ssl_valid = false;
  incomplete_form.preferred = true;
  incomplete_form.blacklisted_by_user = false;
  incomplete_form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_TRUE(db_.AddLogin(incomplete_form));

  // A form on some website. It should trigger a match with the stored one.
  PasswordForm encountered_form;
  encountered_form.origin = GURL("http://accounts.google.com/LoginAuth");
  encountered_form.signon_realm = "http://accounts.google.com/";
  encountered_form.action = GURL("http://accounts.google.com/Login");
  encountered_form.username_element = ASCIIToUTF16("Email");
  encountered_form.password_element = ASCIIToUTF16("Passwd");
  encountered_form.submit_element = ASCIIToUTF16("signIn");

  // Get matches for encountered_form.
  EXPECT_TRUE(db_.GetLogins(encountered_form, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(incomplete_form.origin, result[0]->origin);
  EXPECT_EQ(incomplete_form.signon_realm, result[0]->signon_realm);
  EXPECT_EQ(incomplete_form.username_value, result[0]->username_value);
#if defined(OS_MACOSX) && !defined(OS_IOS)
  // On Mac, passwords are not stored in login database, instead they're in
  // the keychain.
  EXPECT_TRUE(result[0]->password_value.empty());
#else
  EXPECT_EQ(incomplete_form.password_value, result[0]->password_value);
#endif  // OS_MACOSX && !OS_IOS
  EXPECT_TRUE(result[0]->preferred);
  EXPECT_FALSE(result[0]->ssl_valid);

  // We should return empty 'action', 'username_element', 'password_element'
  // and 'submit_element' as we can't be sure if the credentials were entered
  // in this particular form on the page.
  EXPECT_EQ(GURL(), result[0]->action);
  EXPECT_TRUE(result[0]->username_element.empty());
  EXPECT_TRUE(result[0]->password_element.empty());
  EXPECT_TRUE(result[0]->submit_element.empty());
  ClearResults(&result);

  // Let's say this login form worked. Now update the stored credentials with
  // 'action', 'username_element', 'password_element' and 'submit_element' from
  // the encountered form.
  PasswordForm completed_form(incomplete_form);
  completed_form.action = encountered_form.action;
  completed_form.username_element = encountered_form.username_element;
  completed_form.password_element = encountered_form.password_element;
  completed_form.submit_element = encountered_form.submit_element;
  EXPECT_TRUE(db_.UpdateLogin(completed_form, NULL));

  // Get matches for encountered_form again.
  EXPECT_TRUE(db_.GetLogins(encountered_form, &result));
  ASSERT_EQ(1U, result.size());

  // This time we should have all the info available.
  PasswordForm expected_form(completed_form);
#if defined(OS_MACOSX) && !defined(OS_IOS)
  expected_form.password_value.clear();
#endif  // OS_MACOSX && !OS_IOS
  EXPECT_EQ(expected_form, *result[0]);
  ClearResults(&result);
}

TEST_F(LoginDatabaseTest, UpdateOverlappingCredentials) {
  // Save an incomplete form. Note that it only has a few fields set, ex. it's
  // missing 'action', 'username_element' and 'password_element'. Such forms
  // are sometimes inserted during import from other browsers (which may not
  // store this info).
  PasswordForm incomplete_form;
  incomplete_form.origin = GURL("http://accounts.google.com/LoginAuth");
  incomplete_form.signon_realm = "http://accounts.google.com/";
  incomplete_form.username_value = ASCIIToUTF16("my_username");
  incomplete_form.password_value = ASCIIToUTF16("my_password");
  incomplete_form.ssl_valid = false;
  incomplete_form.preferred = true;
  incomplete_form.blacklisted_by_user = false;
  incomplete_form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_TRUE(db_.AddLogin(incomplete_form));

  // Save a complete version of the previous form. Both forms could exist if
  // the user created the complete version before importing the incomplete
  // version from a different browser.
  PasswordForm complete_form = incomplete_form;
  complete_form.action = GURL("http://accounts.google.com/Login");
  complete_form.username_element = ASCIIToUTF16("username_element");
  complete_form.password_element = ASCIIToUTF16("password_element");
  complete_form.submit_element = ASCIIToUTF16("submit");
  EXPECT_TRUE(db_.AddLogin(complete_form));

  // Make sure both passwords exist.
  ScopedVector<autofill::PasswordForm> result;
  EXPECT_TRUE(db_.GetAutofillableLogins(&result.get()));
  ASSERT_EQ(2U, result.size());
  result.clear();

  // Simulate the user changing their password.
  complete_form.password_value = ASCIIToUTF16("new_password");
  EXPECT_TRUE(db_.UpdateLogin(complete_form, NULL));

  // Only one updated form should exist now.
  EXPECT_TRUE(db_.GetAutofillableLogins(&result.get()));
  ASSERT_EQ(1U, result.size());

  PasswordForm expected_form(complete_form);
#if defined(OS_MACOSX) && !defined(OS_IOS)
  // On Mac, passwords are not stored in login database, instead they're in
  // the keychain.
  expected_form.password_value.clear();
#endif  // OS_MACOSX && !OS_IOS
  EXPECT_EQ(expected_form, *result[0]);
}

#if defined(OS_POSIX)
// Only the current user has permission to read the database.
//
// Only POSIX because GetPosixFilePermissions() only exists on POSIX.
// This tests that sql::Connection::set_restrict_to_user() was called,
// and that function is a noop on non-POSIX platforms in any case.
TEST_F(LoginDatabaseTest, FilePermissions) {
  int mode = base::FILE_PERMISSION_MASK;
  EXPECT_TRUE(base::GetPosixFilePermissions(file_, &mode));
  EXPECT_EQ((mode & base::FILE_PERMISSION_USER_MASK), mode);
}
#endif  // defined(OS_POSIX)
