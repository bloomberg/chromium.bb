// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/testing_profile.h"
#include "webkit/glue/password_form.h"

using webkit_glue::PasswordForm;

class PasswordFormManagerTest : public testing::Test {
 public:
  PasswordFormManagerTest() {
  }
  virtual void SetUp() {
    observed_form_.origin = GURL("http://www.google.com/a/LoginAuth");
    observed_form_.action = GURL("http://www.google.com/a/Login");
    observed_form_.username_element = ASCIIToUTF16("Email");
    observed_form_.password_element = ASCIIToUTF16("Passwd");
    observed_form_.submit_element = ASCIIToUTF16("signIn");
    observed_form_.signon_realm = "http://www.google.com";

    saved_match_ = observed_form_;
    saved_match_.origin = GURL("http://www.google.com/a/ServiceLoginAuth");
    saved_match_.action = GURL("http://www.google.com/a/ServiceLogin");
    saved_match_.preferred = true;
    saved_match_.username_value = ASCIIToUTF16("test@gmail.com");
    saved_match_.password_value = ASCIIToUTF16("test1");
    profile_ = new TestingProfile();
  }

  virtual void TearDown() {
    delete profile_;
  }

  PasswordForm* GetPendingCredentials(PasswordFormManager* p) {
    return &p->pending_credentials_;
  }

  void SimulateMatchingPhase(PasswordFormManager* p, bool find_match) {
    // Roll up the state to mock out the matching phase.
    p->state_ = PasswordFormManager::POST_MATCHING_PHASE;
    if (!find_match)
      return;

    PasswordForm* match = new PasswordForm(saved_match_);
    // Heap-allocated form is owned by p.
    p->best_matches_[match->username_value] = match;
    p->preferred_match_ = match;
  }

  bool IgnoredResult(PasswordFormManager* p, PasswordForm* form) {
    return p->IgnoreResult(*form);
  }

  Profile* profile() { return profile_; }

  PasswordForm* observed_form() { return &observed_form_; }
  PasswordForm* saved_match() { return &saved_match_; }

 private:
  PasswordForm observed_form_;
  PasswordForm saved_match_;
  Profile* profile_;
};

TEST_F(PasswordFormManagerTest, TestNewLogin) {
  PasswordFormManager* manager = new PasswordFormManager(
      profile(), NULL, *observed_form(), false);
  SimulateMatchingPhase(manager, false);
  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.preferred = true;
  manager->ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, which should know this is a new login.
  EXPECT_TRUE(manager->IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db.
  EXPECT_EQ(observed_form()->origin.spec(),
            GetPendingCredentials(manager)->origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            GetPendingCredentials(manager)->signon_realm);
  EXPECT_TRUE(GetPendingCredentials(manager)->preferred);
  EXPECT_EQ(saved_match()->password_value,
            GetPendingCredentials(manager)->password_value);
  EXPECT_EQ(saved_match()->username_value,
            GetPendingCredentials(manager)->username_value);

  // Now, suppose the user re-visits the site and wants to save an additional
  // login for the site with a new username. In this case, the matching phase
  // will yield the previously saved login.
  SimulateMatchingPhase(manager, true);
  // Set up the new login.
  string16 new_user = ASCIIToUTF16("newuser");
  string16 new_pass = ASCIIToUTF16("newpass");
  credentials.username_value = new_user;
  credentials.password_value = new_pass;
  manager->ProvisionallySave(credentials);

  // Again, the PasswordFormManager should know this is still a new login.
  EXPECT_TRUE(manager->IsNewLogin());
  // And make sure everything squares up again.
  EXPECT_EQ(observed_form()->origin.spec(),
            GetPendingCredentials(manager)->origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            GetPendingCredentials(manager)->signon_realm);
  EXPECT_TRUE(GetPendingCredentials(manager)->preferred);
  EXPECT_EQ(new_pass,
            GetPendingCredentials(manager)->password_value);
  EXPECT_EQ(new_user,
            GetPendingCredentials(manager)->username_value);
  // Done.
  delete manager;
}

TEST_F(PasswordFormManagerTest, TestUpdatePassword) {
  // Create a PasswordFormManager with observed_form, as if we just
  // saw this form and need to find matching logins.
  PasswordFormManager* manager = new PasswordFormManager(
      profile(), NULL, *observed_form(), false);
  SimulateMatchingPhase(manager, true);

  // User submits credentials for the observed form using a username previously
  // stored, but a new password. Note that the observed form may have different
  // origin URL (as it does in this case) than the saved_match, but we want to
  // make sure the updated password is reflected in saved_match, because that is
  // what we autofilled.
  string16 new_pass = ASCIIToUTF16("newpassword");
  PasswordForm credentials = *observed_form();
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = new_pass;
  credentials.preferred = true;
  manager->ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(manager->IsNewLogin());

  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db. (This verifies correct
  // behaviour for bug 1074420).
  EXPECT_EQ(GetPendingCredentials(manager)->origin.spec(),
            saved_match()->origin.spec());
  EXPECT_EQ(GetPendingCredentials(manager)->signon_realm,
            saved_match()->signon_realm);
  EXPECT_TRUE(GetPendingCredentials(manager)->preferred);
  EXPECT_EQ(new_pass,
            GetPendingCredentials(manager)->password_value);
  // Done.
  delete manager;
}

TEST_F(PasswordFormManagerTest, TestIgnoreResult) {
  PasswordFormManager* manager = new PasswordFormManager(
      profile(), NULL, *observed_form(), false);
  // Make sure we don't match a PasswordForm if it was originally saved on
  // an SSL-valid page and we are now on a page with invalid certificate.
  saved_match()->ssl_valid = true;
  EXPECT_TRUE(IgnoredResult(manager, saved_match()));

  saved_match()->ssl_valid = false;
  // Different paths for action / origin are okay.
  saved_match()->action = GURL("http://www.google.com/b/Login");
  saved_match()->origin = GURL("http://www.google.com/foo");
  EXPECT_FALSE(IgnoredResult(manager, saved_match()));

  // Done.
  delete manager;
}

TEST_F(PasswordFormManagerTest, TestEmptyAction) {
  scoped_ptr<PasswordFormManager> manager(new PasswordFormManager(
      profile(), NULL, *observed_form(), false));

  saved_match()->action = GURL();
  SimulateMatchingPhase(manager.get(), true);
  // User logs in with the autofilled username / password from saved_match.
  PasswordForm login = *observed_form();
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;
  manager->ProvisionallySave(login);
  EXPECT_FALSE(manager->IsNewLogin());
  // We bless our saved PasswordForm entry with the action URL of the
  // observed form.
  EXPECT_EQ(observed_form()->action,
            GetPendingCredentials(manager.get())->action);
}

TEST_F(PasswordFormManagerTest, TestValidForms) {
  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.scheme = PasswordForm::SCHEME_HTML;
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;

  // Form with both username_element and password_element.
  PasswordFormManager manager1(profile(), NULL, credentials, false);
  SimulateMatchingPhase(&manager1, false);
  EXPECT_TRUE(manager1.HasValidPasswordForm());

  // Form without a username_element but with a password_element.
  credentials.username_element.clear();
  PasswordFormManager manager2(profile(), NULL, credentials, false);
  SimulateMatchingPhase(&manager2, false);
  EXPECT_FALSE(manager2.HasValidPasswordForm());

  // Form without a password_element but with a username_element.
  credentials.username_element = saved_match()->username_element;
  credentials.password_element.clear();
  PasswordFormManager manager3(profile(), NULL, credentials, false);
  SimulateMatchingPhase(&manager3, false);
  EXPECT_FALSE(manager3.HasValidPasswordForm());

  // Form with neither a password_element nor a username_element.
  credentials.username_element.clear();
  credentials.password_element.clear();
  PasswordFormManager manager4(profile(), NULL, credentials, false);
  SimulateMatchingPhase(&manager4, false);
  EXPECT_FALSE(manager4.HasValidPasswordForm());
}

TEST_F(PasswordFormManagerTest, TestValidFormsBasic) {
  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.scheme = PasswordForm::SCHEME_BASIC;
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;

  // Form with both username_element and password_element.
  PasswordFormManager manager1(profile(), NULL, credentials, false);
  SimulateMatchingPhase(&manager1, false);
  EXPECT_TRUE(manager1.HasValidPasswordForm());

  // Form without a username_element but with a password_element.
  credentials.username_element.clear();
  PasswordFormManager manager2(profile(), NULL, credentials, false);
  SimulateMatchingPhase(&manager2, false);
  EXPECT_TRUE(manager2.HasValidPasswordForm());

  // Form without a password_element but with a username_element.
  credentials.username_element = saved_match()->username_element;
  credentials.password_element.clear();
  PasswordFormManager manager3(profile(), NULL, credentials, false);
  SimulateMatchingPhase(&manager3, false);
  EXPECT_TRUE(manager3.HasValidPasswordForm());

  // Form with neither a password_element nor a username_element.
  credentials.username_element.clear();
  credentials.password_element.clear();
  PasswordFormManager manager4(profile(), NULL, credentials, false);
  SimulateMatchingPhase(&manager4, false);
  EXPECT_TRUE(manager4.HasValidPasswordForm());
}
