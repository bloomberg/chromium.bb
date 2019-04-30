// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_saver_impl.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::FormFieldData;
using autofill::PasswordForm;
using base::ASCIIToUTF16;
using base::StringPiece;
using testing::_;
using testing::DoAll;
using testing::SaveArg;
using testing::StrictMock;

namespace password_manager {

namespace {

// Creates a dummy observed form with some basic arbitrary values.
PasswordForm CreateObserved() {
  PasswordForm form;
  form.origin = GURL("https://example.in");
  form.signon_realm = form.origin.spec();
  form.action = GURL("https://login.example.org");
  return form;
}

// Creates a dummy pending (for saving) form with some basic arbitrary values
// and |username| and |password| values as specified.
PasswordForm CreatePending(StringPiece username, StringPiece password) {
  PasswordForm form = CreateObserved();
  form.username_value = ASCIIToUTF16(username);
  form.password_value = ASCIIToUTF16(password);
  form.preferred = true;
  return form;
}

}  // namespace

class FormSaverImplTest : public testing::Test {
 public:
  FormSaverImplTest()
      : mock_store_(new StrictMock<MockPasswordStore>()),
        form_saver_(mock_store_.get()) {}

  ~FormSaverImplTest() override { mock_store_->ShutdownOnUIThread(); }

 protected:
  // For the MockPasswordStore.
  base::test::ScopedTaskEnvironment task_environment_;
  scoped_refptr<StrictMock<MockPasswordStore>> mock_store_;
  FormSaverImpl form_saver_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormSaverImplTest);
};

// Check that blacklisting an observed form sets the right properties and calls
// the PasswordStore.
TEST_F(FormSaverImplTest, PermanentlyBlacklist) {
  PasswordForm observed = CreateObserved();
  PasswordForm saved;

  observed.blacklisted_by_user = false;
  observed.preferred = true;
  observed.username_value = ASCIIToUTF16("user1");
  observed.username_element = ASCIIToUTF16("user");
  observed.password_value = ASCIIToUTF16("12345");
  observed.password_element = ASCIIToUTF16("password");
  observed.other_possible_usernames = {
      {ASCIIToUTF16("user2"), ASCIIToUTF16("field")}};
  observed.origin = GURL("https://www.example.com/foobar");

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  form_saver_.PermanentlyBlacklist(&observed);
  EXPECT_TRUE(saved.blacklisted_by_user);
  EXPECT_FALSE(saved.preferred);
  EXPECT_EQ(base::string16(), saved.username_value);
  EXPECT_EQ(base::string16(), saved.username_element);
  EXPECT_EQ(base::string16(), saved.password_value);
  EXPECT_EQ(base::string16(), saved.password_element);
  EXPECT_TRUE(saved.other_possible_usernames.empty());
  EXPECT_EQ(GURL("https://www.example.com/"), saved.origin);
}

// Check that saving the pending form as new adds the credential to the store
// (rather than updating).
TEST_F(FormSaverImplTest, Save_AsNew) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  PasswordForm saved;

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  form_saver_.Save(pending, {} /* matches */, base::string16());
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
}

// Check that saving the pending form as not new updates the store with the
// credential.
TEST_F(FormSaverImplTest, Save_Update) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  PasswordForm saved;

  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  form_saver_.Update(pending, std::map<base::string16, const PasswordForm*>(),
                     nullptr, nullptr);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
}

// Check that passing other credentials to update to the Save call results in
// the store being updated with those credentials in addition to the pending
// one.
TEST_F(FormSaverImplTest, Save_UpdateAlsoOtherCredentials) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  PasswordForm related1 = pending;
  related1.origin = GURL("https://other.example.ca");
  related1.signon_realm = related1.origin.spec();
  PasswordForm related2 = pending;
  related2.origin = GURL("http://complete.example.net");
  related2.signon_realm = related2.origin.spec();
  std::vector<PasswordForm> credentials_to_update = {related1, related2};
  pending.password_value = ASCIIToUTF16("abcd");

  PasswordForm saved[3];

  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLogin(_))
      .WillOnce(SaveArg<0>(&saved[0]))
      .WillOnce(SaveArg<0>(&saved[1]))
      .WillOnce(SaveArg<0>(&saved[2]));
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  form_saver_.Update(pending, std::map<base::string16, const PasswordForm*>(),
                     &credentials_to_update, nullptr);
  std::set<GURL> different_origins;
  for (const PasswordForm& form : saved) {
    different_origins.insert(form.origin);
  }
  EXPECT_THAT(different_origins,
              testing::UnorderedElementsAre(pending.origin, related1.origin,
                                            related2.origin));
}

// Check that if the old primary key is supplied, the appropriate store method
// for update is used.
TEST_F(FormSaverImplTest, Save_UpdateWithPrimaryKey) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  PasswordForm old_key = pending;
  old_key.username_value = ASCIIToUTF16("old username");
  PasswordForm saved_new;
  PasswordForm saved_old;

  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_new), SaveArg<1>(&saved_old)));
  form_saver_.Update(pending, std::map<base::string16, const PasswordForm*>(),
                     nullptr, &old_key);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved_new.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved_new.password_value);
  EXPECT_EQ(ASCIIToUTF16("old username"), saved_old.username_value);
}

// Check that the "preferred" bit of best matches is updated accordingly in the
// store.
TEST_F(FormSaverImplTest, Save_AndUpdatePreferredLoginState) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  pending.preferred = true;

  // |best_matches| will contain 3 forms
  // - non-PSL matched with a different username.
  // - PSL-matched with the same username
  // - another non-PSL that is not preferred.
  // FormSaver should ignore the pending and PSL-matched one, but should update
  // the non-PSL matched form (with different username) to no longer be
  // preferred.
  PasswordForm other = pending;
  other.username_value = ASCIIToUTF16("othername");
  PasswordForm other_non_preferred = CreatePending("other", "wordToP4a55");
  other_non_preferred.preferred = false;
  PasswordForm psl_match = pending;
  psl_match.is_public_suffix_match = true;
  const std::vector<const PasswordForm*> matches = {&other, &psl_match,
                                                    &other_non_preferred};

  PasswordForm saved;
  PasswordForm updated;

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).WillOnce(SaveArg<0>(&updated));
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  form_saver_.Save(pending, matches, base::string16());
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
  EXPECT_TRUE(saved.preferred);
  EXPECT_FALSE(saved.is_public_suffix_match);
  EXPECT_EQ(ASCIIToUTF16("othername"), updated.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), updated.password_value);
  EXPECT_FALSE(updated.preferred);
  EXPECT_FALSE(updated.is_public_suffix_match);
}

// Check that storing credentials with a non-empty username results in deleting
// credentials with the same password but empty username, if present in best
// matches.
TEST_F(FormSaverImplTest, Save_AndDeleteEmptyUsernameCredentials) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  PasswordForm non_empty_username = pending;
  non_empty_username.username_value = ASCIIToUTF16("othername");
  non_empty_username.preferred = false;

  PasswordForm no_username = pending;
  no_username.username_value.clear();
  no_username.preferred = false;
  const std::vector<const PasswordForm*> matches = {&non_empty_username,
                                                    &no_username};

  EXPECT_CALL(*mock_store_, AddLogin(pending));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(no_username));
  form_saver_.Save(pending, matches, base::string16());
}

// Check that storing credentials with a non-empty username does not result in
// deleting credentials with a different password, even if they have no
// username.
TEST_F(FormSaverImplTest,
       Save_AndDoNotDeleteEmptyUsernameCredentialsWithDifferentPassword) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  PasswordForm no_username = pending;
  no_username.username_value.clear();
  no_username.preferred = false;
  no_username.password_value = ASCIIToUTF16("abcd");

  EXPECT_CALL(*mock_store_, AddLogin(pending));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, {&no_username}, base::string16());
}

// Check that if a credential without username is saved, and another credential
// with the same password (and a non-empty username) is present in best matches,
// nothing is deleted.
TEST_F(FormSaverImplTest, Save_EmptyUsernameWillNotCauseDeletion) {
  PasswordForm pending = CreatePending("", "wordToP4a55");

  PasswordForm with_username = pending;
  with_username.username_value = ASCIIToUTF16("nameofuser");
  with_username.preferred = false;

  EXPECT_CALL(*mock_store_, AddLogin(pending));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, {&with_username}, base::string16());
}

// Check that PSL-matched credentials in best matches are exempt from deletion,
// even if they have an empty username and the same password as the pending
// credential.
TEST_F(FormSaverImplTest, Save_AndDoNotDeleteEmptyUsernamePSLCredentials) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  PasswordForm stored = pending;
  PasswordForm no_username_psl = pending;
  no_username_psl.username_value.clear();
  no_username_psl.is_public_suffix_match = true;
  const std::vector<const PasswordForm*> matches = {&stored, &no_username_psl};

  EXPECT_CALL(*mock_store_, AddLogin(pending));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, matches, base::string16());
}

// Check that on storing a credential, other credentials with the same password
// are not removed, as long as they have a non-empty username.
TEST_F(FormSaverImplTest, Save_AndDoNotDeleteNonEmptyUsernameCredentials) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  PasswordForm other_username = pending;
  other_username.username_value = ASCIIToUTF16("other username");
  other_username.preferred = false;

  EXPECT_CALL(*mock_store_, AddLogin(pending));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, {&other_username}, base::string16());
}

// Stores a credential and makes sure that its duplicate is updated.
TEST_F(FormSaverImplTest, Save_AndUpdatePasswordValuesOnExactMatch) {
  constexpr char kOldPassword[] = "old_password";
  constexpr char kNewPassword[] = "new_password";

  PasswordForm duplicate = CreatePending("nameofuser", kOldPassword);
  duplicate.origin = GURL("https://example.in/somePath");

  PasswordForm expected_update = duplicate;
  expected_update.password_value = ASCIIToUTF16(kNewPassword);
  PasswordForm pending = CreatePending("nameofuser", kNewPassword);

  EXPECT_CALL(*mock_store_, AddLogin(pending));
  EXPECT_CALL(*mock_store_, UpdateLogin(expected_update));
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, {&duplicate}, ASCIIToUTF16(kOldPassword));
}

// Stores a credential and makes sure that its PSL duplicate is updated.
TEST_F(FormSaverImplTest, Save_AndUpdatePasswordValuesOnPSLMatch) {
  constexpr char kOldPassword[] = "old_password";
  constexpr char kNewPassword[] = "new_password";
  PasswordForm pending = CreatePending("nameofuser", kOldPassword);

  PasswordForm duplicate = pending;
  duplicate.origin = GURL("https://www.example.in");
  duplicate.signon_realm = duplicate.origin.spec();
  duplicate.is_public_suffix_match = true;

  PasswordForm expected_update = duplicate;
  expected_update.password_value = ASCIIToUTF16(kNewPassword);
  pending.password_value = ASCIIToUTF16(kNewPassword);

  EXPECT_CALL(*mock_store_, AddLogin(pending));
  EXPECT_CALL(*mock_store_, UpdateLogin(expected_update));
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, {&duplicate}, ASCIIToUTF16(kOldPassword));
}

// Stores a credential and makes sure that not exact matches are not updated.
TEST_F(FormSaverImplTest, Save_AndUpdatePasswordValues_IgnoreNonMatches) {
  constexpr char kOldPassword[] = "old_password";
  constexpr char kNewPassword[] = "new_password";
  PasswordForm pending = CreatePending("nameofuser", kOldPassword);

  PasswordForm different_username = pending;
  different_username.username_value = ASCIIToUTF16("someuser");
  different_username.preferred = false;

  PasswordForm different_password = pending;
  different_password.password_value = ASCIIToUTF16("some_password");
  different_password.preferred = false;

  PasswordForm empty_username = pending;
  empty_username.username_value.clear();
  empty_username.preferred = false;
  const std::vector<const PasswordForm*> matches = {
      &different_username, &different_password, &empty_username};

  pending.password_value = ASCIIToUTF16(kNewPassword);

  EXPECT_CALL(*mock_store_, AddLogin(pending));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, matches, ASCIIToUTF16(kOldPassword));
}

// Check that Remove() method is relayed properly.
TEST_F(FormSaverImplTest, Remove) {
  PasswordForm form = CreatePending("nameofuser", "wordToP4a55");

  EXPECT_CALL(*mock_store_, RemoveLogin(form));
  form_saver_.Remove(form);
}

// Check that on saving the pending form |form_data| is sanitized.
TEST_F(FormSaverImplTest, FormDataSanitized) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  FormFieldData field;
  field.name = ASCIIToUTF16("name");
  field.form_control_type = "password";
  field.value = ASCIIToUTF16("value");
  field.label = ASCIIToUTF16("label");
  field.placeholder = ASCIIToUTF16("placeholder");
  field.id_attribute = ASCIIToUTF16("id");
  field.name_attribute = field.name;
  field.css_classes = ASCIIToUTF16("css_classes");
  pending.form_data.fields.push_back(field);

  PasswordForm saved;
  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  form_saver_.Save(pending, {}, base::string16());

  ASSERT_EQ(1u, saved.form_data.fields.size());
  const FormFieldData& saved_field = saved.form_data.fields[0];
  EXPECT_EQ(ASCIIToUTF16("name"), saved_field.name);
  EXPECT_EQ("password", saved_field.form_control_type);
  EXPECT_TRUE(saved_field.value.empty());
  EXPECT_TRUE(saved_field.label.empty());
  EXPECT_TRUE(saved_field.placeholder.empty());
  EXPECT_TRUE(saved_field.id_attribute.empty());
  EXPECT_TRUE(saved_field.name_attribute.empty());
  EXPECT_TRUE(saved_field.css_classes.empty());
}

}  // namespace password_manager
