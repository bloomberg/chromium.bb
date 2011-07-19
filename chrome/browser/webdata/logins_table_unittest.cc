// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/password_form.h"

using base::Time;
using base::TimeDelta;
using webkit_glue::PasswordForm;

class LoginsTableTest : public testing::Test {
 public:
  LoginsTableTest() {}
  virtual ~LoginsTableTest() {}

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

  FilePath file_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginsTableTest);
};

TEST_F(LoginsTableTest, Logins) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
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
  EXPECT_TRUE(db.GetLoginsTable()->AddLogin(form));
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db.GetLoginsTable()->GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The example site changes...
  PasswordForm form2(form);
  form2.origin = GURL("http://www.google.com/new/accounts/LoginAuth");
  form2.submit_element = ASCIIToUTF16("reallySignIn");

  // Match against an inexact copy
  EXPECT_TRUE(db.GetLoginsTable()->GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Uh oh, the site changed origin & action URL's all at once!
  PasswordForm form3(form2);
  form3.action = GURL("http://www.google.com/new/accounts/Login");

  // signon_realm is the same, should match.
  EXPECT_TRUE(db.GetLoginsTable()->GetLogins(form3, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Imagine the site moves to a secure server for login.
  PasswordForm form4(form3);
  form4.signon_realm = "https://www.google.com/";
  form4.ssl_valid = true;

  // We have only an http record, so no match for this.
  EXPECT_TRUE(db.GetLoginsTable()->GetLogins(form4, &result));
  EXPECT_EQ(0U, result.size());

  // Let's imagine the user logs into the secure site.
  EXPECT_TRUE(db.GetLoginsTable()->AddLogin(form4));
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
  EXPECT_EQ(2U, result.size());
  delete result[0];
  delete result[1];
  result.clear();

  // Now the match works
  EXPECT_TRUE(db.GetLoginsTable()->GetLogins(form4, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The user chose to forget the original but not the new.
  EXPECT_TRUE(db.GetLoginsTable()->RemoveLogin(form));
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The old form wont match the new site (http vs https).
  EXPECT_TRUE(db.GetLoginsTable()->GetLogins(form, &result));
  EXPECT_EQ(0U, result.size());

  // The user's request for the HTTPS site is intercepted
  // by an attacker who presents an invalid SSL cert.
  PasswordForm form5(form4);
  form5.ssl_valid = 0;

  // It will match in this case.
  EXPECT_TRUE(db.GetLoginsTable()->GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // User changes his password.
  PasswordForm form6(form5);
  form6.password_value = ASCIIToUTF16("test6");
  form6.preferred = true;

  // We update, and check to make sure it matches the
  // old form, and there is only one record.
  EXPECT_TRUE(db.GetLoginsTable()->UpdateLogin(form6));
  // matches
  EXPECT_TRUE(db.GetLoginsTable()->GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();
  // Only one record.
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  // password element was updated.
  EXPECT_EQ(form6.password_value, result[0]->password_value);
  // Preferred login.
  EXPECT_TRUE(form6.preferred);
  delete result[0];
  result.clear();

  // Make sure everything can disappear.
  EXPECT_TRUE(db.GetLoginsTable()->RemoveLogin(form4));
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());
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
  return db->GetLoginsTable()->AddLogin(form);
}

static void ClearResults(std::vector<PasswordForm*>* results) {
  for (size_t i = 0; i < results->size(); ++i) {
    delete (*results)[i];
  }
  results->clear();
}

TEST_F(LoginsTableTest, ClearPrivateData_SavedPasswords) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
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
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
  EXPECT_EQ(4U, result.size());
  ClearResults(&result);

  // Delete everything from today's date and on.
  db.GetLoginsTable()->RemoveLoginsCreatedBetween(now, Time());

  // Should have deleted half of what we inserted.
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
  EXPECT_EQ(2U, result.size());
  ClearResults(&result);

  // Delete with 0 date (should delete all).
  db.GetLoginsTable()->RemoveLoginsCreatedBetween(Time(), Time());

  // Verify nothing is left.
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginsTableTest, BlacklistedLogins) {
  WebDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_));
  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
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
  EXPECT_TRUE(db.GetLoginsTable()->AddLogin(form));

  // Get all non-blacklisted logins (should be none).
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, false));
  ASSERT_EQ(0U, result.size());

  // GetLogins should give the blacklisted result.
  EXPECT_TRUE(db.GetLoginsTable()->GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  ClearResults(&result);

  // So should GetAll including blacklisted.
  EXPECT_TRUE(db.GetLoginsTable()->GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  ClearResults(&result);
}
