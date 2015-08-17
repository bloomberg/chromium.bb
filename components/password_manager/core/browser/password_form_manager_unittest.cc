// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace password_manager {

namespace {

// Invokes the password store consumer with a copy of all forms.
ACTION_P4(InvokeConsumer, form1, form2, form3, form4) {
  ScopedVector<PasswordForm> result;
  result.push_back(make_scoped_ptr(new PasswordForm(form1)));
  result.push_back(make_scoped_ptr(new PasswordForm(form2)));
  result.push_back(make_scoped_ptr(new PasswordForm(form3)));
  result.push_back(make_scoped_ptr(new PasswordForm(form4)));
  arg0->OnGetPasswordStoreResults(result.Pass());
}

MATCHER_P(CheckUsername, username_value, "Username incorrect") {
  return arg.username_value == username_value;
}

void ClearVector(ScopedVector<PasswordForm>* results) {
  results->clear();
}

class MockAutofillManager : public autofill::AutofillManager {
 public:
  MockAutofillManager(autofill::AutofillDriver* driver,
                      autofill::AutofillClient* client,
                      autofill::PersonalDataManager* data_manager)
      : AutofillManager(driver, client, data_manager) {}

  MOCK_METHOD4(UploadPasswordForm,
               bool(const autofill::FormData&,
                    const base::string16&,
                    const autofill::ServerFieldType&,
                    const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillManager);
};

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver()
      : mock_autofill_manager_(&test_autofill_driver_,
                               &test_autofill_client_,
                               &test_personal_data_manager_) {}

  ~MockPasswordManagerDriver() {}

  MOCK_METHOD1(AllowPasswordGenerationForForm,
               void(const autofill::PasswordForm&));

  MockAutofillManager* mock_autofill_manager() {
    return &mock_autofill_manager_;
  }

 private:
  autofill::TestAutofillDriver test_autofill_driver_;
  autofill::TestAutofillClient test_autofill_client_;
  autofill::TestPersonalDataManager test_personal_data_manager_;

  NiceMock<MockAutofillManager> mock_autofill_manager_;
};

class MockStoreResultFilter : public CredentialsFilter {
 public:
  MOCK_CONST_METHOD1(FilterResultsPtr,
                     void(ScopedVector<autofill::PasswordForm>* results));

  // GMock cannot handle move-only arguments.
  ScopedVector<autofill::PasswordForm> FilterResults(
      ScopedVector<autofill::PasswordForm> results) const override {
    FilterResultsPtr(&results);
    return results.Pass();
  }
};

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  explicit TestPasswordManagerClient(PasswordStore* password_store)
      : password_store_(password_store),
        driver_(new NiceMock<MockPasswordManagerDriver>),
        is_update_password_ui_enabled_(false),
        filter_all_results_(false) {
    prefs_.registry()->RegisterBooleanPref(prefs::kPasswordManagerSavingEnabled,
                                           true);
  }

  // After this method is called, the filter returned by CreateStoreResultFilter
  // will filter out all forms.
  void FilterAllResults() { filter_all_results_ = true; }

  scoped_ptr<CredentialsFilter> CreateStoreResultFilter() const override {
    scoped_ptr<NiceMock<MockStoreResultFilter>> stub_filter(
        new NiceMock<MockStoreResultFilter>);
    if (filter_all_results_) {
      // EXPECT_CALL rather than ON_CALL, because if the test needs the
      // filtering, then it needs it called.
      EXPECT_CALL(*stub_filter, FilterResultsPtr(_))
          .WillRepeatedly(testing::Invoke(ClearVector));
    }
    return stub_filter.Pass();
  }

  PrefService* GetPrefs() override { return &prefs_; }
  PasswordStore* GetPasswordStore() const override { return password_store_; }

  MockPasswordManagerDriver* mock_driver() { return driver_.get(); }

  base::WeakPtr<PasswordManagerDriver> driver() { return driver_->AsWeakPtr(); }

  autofill::AutofillManager* GetAutofillManagerForMainFrame() override {
    return mock_driver()->mock_autofill_manager();
  }

  void KillDriver() { driver_.reset(); }

  bool IsUpdatePasswordUIEnabled() const override {
    return is_update_password_ui_enabled_;
  }
  void set_is_update_password_ui_enabled(bool value) {
    is_update_password_ui_enabled_ = value;
  }

 private:
  TestingPrefServiceSimple prefs_;
  PasswordStore* password_store_;
  scoped_ptr<MockPasswordManagerDriver> driver_;
  bool is_update_password_ui_enabled_;
  bool filter_all_results_;
};

class TestPasswordManager : public PasswordManager {
 public:
  explicit TestPasswordManager(TestPasswordManagerClient* client)
      : PasswordManager(client), wait_for_username_(false) {}

  void Autofill(password_manager::PasswordManagerDriver* driver,
                const autofill::PasswordForm& form_for_autofill,
                const autofill::PasswordFormMap& best_matches,
                const autofill::PasswordForm& preferred_match,
                bool wait_for_username) const override {
    best_matches_ = &best_matches;
    wait_for_username_ = wait_for_username;
  }

  const autofill::PasswordFormMap& GetLatestBestMatches() {
    return *best_matches_;
  }

  bool GetLatestWaitForUsername() { return wait_for_username_; }

 private:
  // Points to a PasswordFormMap owned by PasswordFormManager.
  // Marked mutable to get around constness of Autofill().
  // TODO(vabr): This should be rewritten as a mock of PasswordManager, and the
  // interesting arguments should be saved via GMock actions instead.
  mutable const autofill::PasswordFormMap* best_matches_;
  mutable bool wait_for_username_;
};

}  // namespace

class PasswordFormManagerTest : public testing::Test {
 public:
  PasswordFormManagerTest() {}

  // Types of possible outcomes of simulated matching, see
  // SimulateMatchingPhase.
  enum ResultOfSimulatedMatching { RESULT_MATCH_FOUND, RESULT_NO_MATCH };

  void SetUp() override {
    observed_form_.origin = GURL("http://accounts.google.com/a/LoginAuth");
    observed_form_.action = GURL("http://accounts.google.com/a/Login");
    observed_form_.username_element = ASCIIToUTF16("Email");
    observed_form_.password_element = ASCIIToUTF16("Passwd");
    observed_form_.submit_element = ASCIIToUTF16("signIn");
    observed_form_.signon_realm = "http://accounts.google.com";

    saved_match_ = observed_form_;
    saved_match_.origin = GURL("http://accounts.google.com/a/ServiceLoginAuth");
    saved_match_.action = GURL("http://accounts.google.com/a/ServiceLogin");
    saved_match_.preferred = true;
    saved_match_.username_value = ASCIIToUTF16("test@gmail.com");
    saved_match_.password_value = ASCIIToUTF16("test1");
    saved_match_.other_possible_usernames.push_back(
        ASCIIToUTF16("test2@gmail.com"));

    autofill::FormFieldData field;
    field.label = ASCIIToUTF16("full_name");
    field.name = ASCIIToUTF16("full_name");
    field.form_control_type = "text";
    saved_match_.form_data.fields.push_back(field);

    field.label = ASCIIToUTF16("Email");
    field.name = ASCIIToUTF16("Email");
    field.form_control_type = "text";
    saved_match_.form_data.fields.push_back(field);

    field.label = ASCIIToUTF16("password");
    field.name = ASCIIToUTF16("password");
    field.form_control_type = "password";
    saved_match_.form_data.fields.push_back(field);

    mock_store_ = new NiceMock<MockPasswordStore>();
    client_.reset(new TestPasswordManagerClient(mock_store_.get()));
    password_manager_.reset(new TestPasswordManager(client_.get()));
  }

  void TearDown() override {
    if (mock_store_.get())
      mock_store_->Shutdown();
  }

  MockPasswordStore* mock_store() const { return mock_store_.get(); }

  void SimulateMatchingPhase(PasswordFormManager* p,
                             ResultOfSimulatedMatching result) {
    // TODO(vabr): This should use the mock store instead of private methods of
    // PasswordFormManager.
    // Roll up the state to mock out the matching phase.
    p->state_ = PasswordFormManager::POST_MATCHING_PHASE;
    if (result == RESULT_NO_MATCH)
      return;

    scoped_ptr<PasswordForm> match(new PasswordForm(saved_match_));
    p->preferred_match_ = match.get();
    base::string16 username = match->username_value;
    p->best_matches_.insert(username, match.Pass());
  }

  void SanitizePossibleUsernames(PasswordFormManager* p, PasswordForm* form) {
    p->SanitizePossibleUsernames(form);
  }

  // Save saved_match() for observed_form() where |observed_form_data|,
  // |times_used|, and |status| are used to overwrite the default values for
  // observed_form(). |field_type| is the upload that we expect from saving,
  // with nullptr meaning no upload expected.
  void AccountCreationUploadTest(const autofill::FormData& observed_form_data,
                                 int times_used,
                                 PasswordForm::GenerationUploadStatus status,
                                 const autofill::ServerFieldType* field_type) {
    PasswordForm form(*observed_form());

    form.form_data = observed_form_data;

    PasswordFormManager form_manager(password_manager(), client(),
                                     client()->driver(), form, false);
    ScopedVector<PasswordForm> result;
    result.push_back(CreateSavedMatch(false));
    result[0]->generation_upload_status = status;
    result[0]->times_used = times_used;
    result[0]->username_element = ASCIIToUTF16("matched-form-username-field");

    PasswordForm form_to_save(form);
    form_to_save.preferred = true;
    form_to_save.username_element = ASCIIToUTF16("observed-username-field");
    form_to_save.username_value = result[0]->username_value;
    form_to_save.password_value = result[0]->password_value;

    // When we're voting for an account creation form, we should also vote
    // for its username field.
    base::string16 username_vote =
        (field_type && *field_type == autofill::ACCOUNT_CREATION_PASSWORD)
            ? result[0]->username_element
            : base::string16();

    form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
    form_manager.OnGetPasswordStoreResults(result.Pass());
    std::string expected_login_signature;
    autofill::FormStructure observed_structure(observed_form_data);
    autofill::FormStructure pending_structure(saved_match()->form_data);
    if (observed_structure.FormSignature() !=
            pending_structure.FormSignature() &&
        times_used == 0) {
      expected_login_signature = observed_structure.FormSignature();
    }
    if (field_type) {
      EXPECT_CALL(*client()->mock_driver()->mock_autofill_manager(),
                  UploadPasswordForm(_, username_vote, *field_type,
                                     expected_login_signature))
          .Times(1);
    } else {
      EXPECT_CALL(*client()->mock_driver()->mock_autofill_manager(),
                  UploadPasswordForm(_, _, _, _))
          .Times(0);
    }
    form_manager.ProvisionallySave(
        form_to_save, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
    form_manager.Save();
  }

  PasswordForm* observed_form() { return &observed_form_; }
  PasswordForm* saved_match() { return &saved_match_; }
  PasswordForm* CreateSavedMatch(bool blacklisted) {
    // Owned by the caller of this method.
    PasswordForm* match = new PasswordForm(saved_match_);
    match->blacklisted_by_user = blacklisted;
    return match;
  }

  TestPasswordManagerClient* client() { return client_.get(); }

  TestPasswordManager* password_manager() { return password_manager_.get(); }

  // To spare typing for PasswordFormManager instances which need no driver.
  const base::WeakPtr<PasswordManagerDriver> kNoDriver;

 private:
  // Necessary for callbacks, and for TestAutofillDriver.
  base::MessageLoop message_loop_;

  PasswordForm observed_form_;
  PasswordForm saved_match_;
  scoped_refptr<NiceMock<MockPasswordStore>> mock_store_;
  scoped_ptr<TestPasswordManagerClient> client_;
  scoped_ptr<TestPasswordManager> password_manager_;
};

TEST_F(PasswordFormManagerTest, TestNewLogin) {
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);
  SimulateMatchingPhase(&form_manager, RESULT_NO_MATCH);

  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.preferred = true;
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, which should know this is a new login.
  EXPECT_TRUE(form_manager.IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db.
  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager.pending_credentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager.pending_credentials().signon_realm);
  EXPECT_EQ(observed_form()->action, form_manager.pending_credentials().action);
  EXPECT_TRUE(form_manager.pending_credentials().preferred);
  EXPECT_EQ(saved_match()->password_value,
            form_manager.pending_credentials().password_value);
  EXPECT_EQ(saved_match()->username_value,
            form_manager.pending_credentials().username_value);
  EXPECT_TRUE(form_manager.pending_credentials().new_password_element.empty());
  EXPECT_TRUE(form_manager.pending_credentials().new_password_value.empty());

  // Now, suppose the user re-visits the site and wants to save an additional
  // login for the site with a new username. In this case, the matching phase
  // will yield the previously saved login.
  SimulateMatchingPhase(&form_manager, RESULT_MATCH_FOUND);
  // Set up the new login.
  base::string16 new_user = ASCIIToUTF16("newuser");
  base::string16 new_pass = ASCIIToUTF16("newpass");
  credentials.username_value = new_user;
  credentials.password_value = new_pass;
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Again, the PasswordFormManager should know this is still a new login.
  EXPECT_TRUE(form_manager.IsNewLogin());
  // And make sure everything squares up again.
  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager.pending_credentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager.pending_credentials().signon_realm);
  EXPECT_TRUE(form_manager.pending_credentials().preferred);
  EXPECT_EQ(new_pass, form_manager.pending_credentials().password_value);
  EXPECT_EQ(new_user, form_manager.pending_credentials().username_value);
  EXPECT_TRUE(form_manager.pending_credentials().new_password_element.empty());
  EXPECT_TRUE(form_manager.pending_credentials().new_password_value.empty());
}

// If PSL-matched credentials had been suggested, but the user has overwritten
// the password, the provisionally saved credentials should no longer be
// considered as PSL-matched, so that the exception for not prompting before
// saving PSL-matched credentials should no longer apply.
TEST_F(PasswordFormManagerTest,
       OverriddenPSLMatchedCredentialsNotMarkedAsPSLMatched) {
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);

  // The suggestion needs to be PSL-matched.
  saved_match()->original_signon_realm = "www.example.org";
  SimulateMatchingPhase(&form_manager, RESULT_MATCH_FOUND);

  // User modifies the suggested password and submits the form.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value =
      saved_match()->password_value + ASCIIToUTF16("modify");
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_TRUE(form_manager.IsNewLogin());
  EXPECT_FALSE(form_manager.IsPendingCredentialsPublicSuffixMatch());
}

TEST_F(PasswordFormManagerTest, PSLMatchedCredentialsMetadataUpdated) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  // The suggestion needs to be PSL-matched.
  saved_match()->original_signon_realm = "www.example.org";
  SimulateMatchingPhase(&form_manager, RESULT_MATCH_FOUND);

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;
  form_manager.ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  PasswordForm expected_saved_form(submitted_form);
  expected_saved_form.times_used = 1;
  expected_saved_form.other_possible_usernames.clear();
  expected_saved_form.form_data = saved_match()->form_data;
  expected_saved_form.origin = saved_match()->origin;
  expected_saved_form.original_signon_realm =
      saved_match()->original_signon_realm;
  PasswordForm actual_saved_form;

  EXPECT_CALL(*(client()->mock_driver()->mock_autofill_manager()),
              UploadPasswordForm(_, _, autofill::ACCOUNT_CREATION_PASSWORD, _))
      .Times(1);
  EXPECT_CALL(*mock_store(), AddLogin(_))
      .WillOnce(SaveArg<0>(&actual_saved_form));
  form_manager.Save();

  // Can't verify time, so ignore it.
  actual_saved_form.date_created = base::Time();
  EXPECT_EQ(expected_saved_form, actual_saved_form);
}

TEST_F(PasswordFormManagerTest, TestNewLoginFromNewPasswordElement) {
  // Add a new password field to the test form. The PasswordFormManager should
  // save the password from this field, instead of the current password field.
  observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");
  observed_form()->username_marked_by_site = true;

  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);
  SimulateMatchingPhase(&form_manager, RESULT_NO_MATCH);

  // User enters current and new credentials to the observed form.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_value = ASCIIToUTF16("newpassword");
  credentials.preferred = true;
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, which should know this is a new login.
  EXPECT_TRUE(form_manager.IsNewLogin());
  EXPECT_EQ(credentials.origin, form_manager.pending_credentials().origin);
  EXPECT_EQ(credentials.signon_realm,
            form_manager.pending_credentials().signon_realm);
  EXPECT_EQ(credentials.action, form_manager.pending_credentials().action);
  EXPECT_TRUE(form_manager.pending_credentials().preferred);
  EXPECT_EQ(credentials.username_value,
            form_manager.pending_credentials().username_value);

  // By this point, the PasswordFormManager should have promoted the new
  // password value to be the current password, and should have wiped the
  // password element name: it is likely going to be different on a login
  // form, so it is not worth remembering them.
  EXPECT_EQ(credentials.new_password_value,
            form_manager.pending_credentials().password_value);
  EXPECT_TRUE(form_manager.pending_credentials().password_element.empty());
  EXPECT_TRUE(form_manager.pending_credentials().new_password_value.empty());
}

TEST_F(PasswordFormManagerTest, TestUpdatePassword) {
  // Create a PasswordFormManager with observed_form, as if we just
  // saw this form and need to find matching logins.
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);

  SimulateMatchingPhase(&form_manager, RESULT_MATCH_FOUND);

  // User submits credentials for the observed form using a username previously
  // stored, but a new password. Note that the observed form may have different
  // origin URL (as it does in this case) than the saved_match, but we want to
  // make sure the updated password is reflected in saved_match, because that is
  // what we autofilled.
  base::string16 new_pass = ASCIIToUTF16("test2");
  PasswordForm credentials = *observed_form();
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = new_pass;
  credentials.preferred = true;
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());

  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db. (This verifies correct
  // behaviour for bug 1074420).
  EXPECT_EQ(form_manager.pending_credentials().origin.spec(),
            saved_match()->origin.spec());
  EXPECT_EQ(form_manager.pending_credentials().signon_realm,
            saved_match()->signon_realm);
  EXPECT_TRUE(form_manager.pending_credentials().preferred);
  EXPECT_EQ(new_pass, form_manager.pending_credentials().password_value);
}

TEST_F(PasswordFormManagerTest, TestUpdatePasswordFromNewPasswordElement) {
  // Add a new password field to the test form. The PasswordFormManager should
  // save the password from this field, instead of the current password field.
  observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");

  // Given that |observed_form| was most likely a change password form, it
  // should not serve as a source for updating meta-information stored with the
  // old credentials, such as element names, as they are likely going to be
  // different between change password and login forms. To test this in depth,
  // forcibly wipe |submit_element|, which should normally trigger updating this
  // field from |observed_form| in the UpdateLogin() step as a special case. We
  // will verify in the end that this did not happen.
  saved_match()->submit_element.clear();

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  SimulateMatchingPhase(&form_manager, RESULT_MATCH_FOUND);

  // User submits current and new credentials to the observed form.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_value = ASCIIToUTF16("test2");
  credentials.preferred = true;
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());

  // By now, the PasswordFormManager should have promoted the new password value
  // already to be the current password, and should no longer maintain any info
  // about the new password.
  EXPECT_EQ(credentials.new_password_value,
            form_manager.pending_credentials().password_value);
  EXPECT_TRUE(form_manager.pending_credentials().new_password_element.empty());
  EXPECT_TRUE(form_manager.pending_credentials().new_password_value.empty());

  // Trigger saving to exercise some special case handling in UpdateLogin().
  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  form_manager.Save();
  Mock::VerifyAndClearExpectations(mock_store());

  // No meta-information should be updated, only the password.
  EXPECT_EQ(credentials.new_password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_EQ(saved_match()->submit_element, new_credentials.submit_element);
}

TEST_F(PasswordFormManagerTest, TestIgnoreResult_SSL) {
  PasswordForm observed(*observed_form());
  observed.origin = GURL("https://accounts.google.com/a/LoginAuth");
  observed.action = GURL("https://accounts.google.com/a/Login");
  observed.signon_realm = "https://accounts.google.com";
  const bool kObservedFormSSLValid = false;
  observed.ssl_valid = kObservedFormSSLValid;

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), observed,
                                   kObservedFormSSLValid);

  PasswordForm saved_form = observed;
  saved_form.ssl_valid = true;
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  // Make sure we don't match a PasswordForm if it was originally saved on
  // an SSL-valid page and we are now on a page with invalid certificate.
  EXPECT_TRUE(form_manager.best_matches().empty());
}

TEST_F(PasswordFormManagerTest, TestIgnoreResult_Paths) {
  PasswordForm observed(*observed_form());
  observed.origin = GURL("https://accounts.google.com/a/LoginAuth");
  observed.action = GURL("https://accounts.google.com/a/Login");
  observed.signon_realm = "https://accounts.google.com";

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), observed, false);

  PasswordForm saved_form = observed;
  saved_form.origin = GURL("https://accounts.google.com/a/OtherLoginAuth");
  saved_form.action = GURL("https://accounts.google.com/a/OtherLogin");
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  // Different paths for action / origin are okay.
  EXPECT_FALSE(form_manager.best_matches().empty());
}

TEST_F(PasswordFormManagerTest, TestIgnoreResult_IgnoredCredentials) {
  PasswordForm observed(*observed_form());
  observed.origin = GURL("https://accounts.google.com/a/LoginAuth");
  observed.action = GURL("https://accounts.google.com/a/Login");
  observed.signon_realm = "https://accounts.google.com";

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), observed, false);
  client()->FilterAllResults();

  PasswordForm saved_form = observed;
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  // Results should be ignored if the client requests it.
  EXPECT_TRUE(form_manager.best_matches().empty());
}

TEST_F(PasswordFormManagerTest, TestEmptyAction) {
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);

  saved_match()->action = GURL();
  SimulateMatchingPhase(&form_manager, RESULT_MATCH_FOUND);
  // User logs in with the autofilled username / password from saved_match.
  PasswordForm login = *observed_form();
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;
  form_manager.ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_FALSE(form_manager.IsNewLogin());
  // We bless our saved PasswordForm entry with the action URL of the
  // observed form.
  EXPECT_EQ(observed_form()->action, form_manager.pending_credentials().action);
}

TEST_F(PasswordFormManagerTest, TestUpdateAction) {
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);

  SimulateMatchingPhase(&form_manager, RESULT_MATCH_FOUND);
  // User logs in with the autofilled username / password from saved_match.
  PasswordForm login = *observed_form();
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;

  form_manager.ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_FALSE(form_manager.IsNewLogin());
  // The observed action URL is different from the previously saved one, and
  // is the same as the one that would be submitted on successful login.
  EXPECT_NE(observed_form()->action, saved_match()->action);
  EXPECT_EQ(observed_form()->action, form_manager.pending_credentials().action);
}

TEST_F(PasswordFormManagerTest, TestDynamicAction) {
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);

  SimulateMatchingPhase(&form_manager, RESULT_NO_MATCH);
  PasswordForm login(*observed_form());
  // The submitted action URL is different from the one observed on page load.
  GURL new_action = GURL("http://www.google.com/new_action");
  login.action = new_action;

  form_manager.ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_TRUE(form_manager.IsNewLogin());
  // Check that the provisionally saved action URL is the same as the submitted
  // action URL, not the one observed on page load.
  EXPECT_EQ(new_action, form_manager.pending_credentials().action);
}

TEST_F(PasswordFormManagerTest, TestAlternateUsername_NoChange) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  EXPECT_CALL(*client()->mock_driver(), AllowPasswordGenerationForForm(_));

  PasswordForm saved_form = *saved_match();
  ASSERT_FALSE(saved_form.other_possible_usernames.empty());

  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  // The saved match has the right username already.
  PasswordForm login(*observed_form());
  login.preferred = true;
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;

  form_manager.ProvisionallySave(
      login, PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(form_manager.IsNewLogin());

  PasswordForm saved_result;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(SaveArg<0>(&saved_result));
  EXPECT_CALL(*mock_store(), UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_)).Times(0);

  form_manager.Save();
  // Should be only one password stored, and should not have
  // |other_possible_usernames| set anymore.
  EXPECT_EQ(saved_match()->username_value, saved_result.username_value);
  EXPECT_TRUE(saved_result.other_possible_usernames.empty());
}

TEST_F(PasswordFormManagerTest, TestAlternateUsername_OtherUsername) {
  // This time use an alternate username.
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  EXPECT_CALL(*client()->mock_driver(), AllowPasswordGenerationForForm(_));

  PasswordForm saved_form = *saved_match();
  ASSERT_FALSE(saved_form.other_possible_usernames.empty());

  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  // The saved match has the right username already.
  PasswordForm login(*observed_form());
  login.preferred = true;
  login.username_value = saved_match()->other_possible_usernames[0];
  login.password_value = saved_match()->password_value;

  form_manager.ProvisionallySave(
      login, PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(form_manager.IsNewLogin());

  PasswordForm saved_result;
  // Changing the username changes the primary key of the stored credential.
  EXPECT_CALL(*mock_store(), UpdateLoginWithPrimaryKey(
                                 _, CheckUsername(saved_form.username_value)))
      .WillOnce(SaveArg<0>(&saved_result));
  EXPECT_CALL(*mock_store(), UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_)).Times(0);

  form_manager.Save();

  // |other_possible_usernames| should also be empty, but username_value should
  // be changed to match |new_username|.
  EXPECT_EQ(saved_match()->other_possible_usernames[0],
            saved_result.username_value);
  EXPECT_TRUE(saved_result.other_possible_usernames.empty());
}

TEST_F(PasswordFormManagerTest, TestValidForms) {
  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.scheme = PasswordForm::SCHEME_HTML;
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;

  // An alternate version of the form that also has a new_password_element.
  PasswordForm new_credentials(*observed_form());
  new_credentials.new_password_element = ASCIIToUTF16("NewPasswd");
  new_credentials.new_password_value = ASCIIToUTF16("test1new");

  // Form with both username_element and password_element.
  PasswordFormManager manager1(password_manager(), nullptr, kNoDriver,
                               credentials, false);
  SimulateMatchingPhase(&manager1, RESULT_NO_MATCH);
  EXPECT_TRUE(manager1.HasValidPasswordForm());

  // Form with username_element, password_element, and new_password_element.
  PasswordFormManager manager2(password_manager(), nullptr, kNoDriver,
                               new_credentials, false);
  SimulateMatchingPhase(&manager2, RESULT_NO_MATCH);
  EXPECT_TRUE(manager2.HasValidPasswordForm());

  // Form with username_element and only new_password_element.
  new_credentials.password_element.clear();
  PasswordFormManager manager3(password_manager(), nullptr, kNoDriver,
                               new_credentials, false);
  SimulateMatchingPhase(&manager3, RESULT_NO_MATCH);
  EXPECT_TRUE(manager3.HasValidPasswordForm());

  // Form without a username_element but with a password_element.
  credentials.username_element.clear();
  PasswordFormManager manager4(password_manager(), nullptr, kNoDriver,
                               credentials, false);
  SimulateMatchingPhase(&manager4, RESULT_NO_MATCH);
  EXPECT_TRUE(manager4.HasValidPasswordForm());

  // Form without a username_element but with a new_password_element.
  new_credentials.username_element.clear();
  PasswordFormManager manager5(password_manager(), nullptr, kNoDriver,
                               new_credentials, false);
  SimulateMatchingPhase(&manager5, RESULT_NO_MATCH);
  EXPECT_TRUE(manager5.HasValidPasswordForm());

  // Form without a password_element but with a username_element.
  credentials.username_element = saved_match()->username_element;
  credentials.password_element.clear();
  PasswordFormManager manager6(password_manager(), nullptr, kNoDriver,
                               credentials, false);
  SimulateMatchingPhase(&manager6, RESULT_NO_MATCH);
  EXPECT_FALSE(manager6.HasValidPasswordForm());

  // Form with neither a password_element nor a username_element.
  credentials.username_element.clear();
  credentials.password_element.clear();
  PasswordFormManager manager7(password_manager(), nullptr, kNoDriver,
                               credentials, false);
  SimulateMatchingPhase(&manager7, RESULT_NO_MATCH);
  EXPECT_FALSE(manager7.HasValidPasswordForm());
}

TEST_F(PasswordFormManagerTest, TestValidFormsBasic) {
  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.scheme = PasswordForm::SCHEME_BASIC;
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;

  // Form with both username_element and password_element.
  PasswordFormManager manager1(password_manager(), nullptr, kNoDriver,
                               credentials, false);
  SimulateMatchingPhase(&manager1, RESULT_NO_MATCH);
  EXPECT_TRUE(manager1.HasValidPasswordForm());

  // Form without a username_element but with a password_element.
  credentials.username_element.clear();
  PasswordFormManager manager2(password_manager(), nullptr, kNoDriver,
                               credentials, false);
  SimulateMatchingPhase(&manager2, RESULT_NO_MATCH);
  EXPECT_TRUE(manager2.HasValidPasswordForm());

  // Form without a password_element but with a username_element.
  credentials.username_element = saved_match()->username_element;
  credentials.password_element.clear();
  PasswordFormManager manager3(password_manager(), nullptr, kNoDriver,
                               credentials, false);
  SimulateMatchingPhase(&manager3, RESULT_NO_MATCH);
  EXPECT_TRUE(manager3.HasValidPasswordForm());

  // Form with neither a password_element nor a username_element.
  credentials.username_element.clear();
  credentials.password_element.clear();
  PasswordFormManager manager4(password_manager(), nullptr, kNoDriver,
                               credentials, false);
  SimulateMatchingPhase(&manager4, RESULT_NO_MATCH);
  EXPECT_TRUE(manager4.HasValidPasswordForm());
}

TEST_F(PasswordFormManagerTest, TestSendNotBlacklistedMessage) {
  PasswordFormManager manager_no_creds(password_manager(), client(),
                                       client()->driver(), *observed_form(),
                                       false);

  // First time sign-up attempt. Password store does not contain matching
  // credentials. AllowPasswordGenerationForForm should be called to send the
  // "not blacklisted" message.
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);
  manager_no_creds.SimulateFetchMatchingLoginsFromPasswordStore();
  manager_no_creds.OnGetPasswordStoreResults(ScopedVector<PasswordForm>());
  Mock::VerifyAndClearExpectations(client()->mock_driver());

  // Signing up on a previously visited site. Credentials are found in the
  // password store, and are not blacklisted. AllowPasswordGenerationForForm
  // should be called to send the "not blacklisted" message.
  PasswordFormManager manager_creds(password_manager(), client(),
                                    client()->driver(), *observed_form(),
                                    false);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);
  manager_creds.SimulateFetchMatchingLoginsFromPasswordStore();
  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(CreateSavedMatch(false));
  manager_creds.OnGetPasswordStoreResults(simulated_results.Pass());
  Mock::VerifyAndClearExpectations(client()->mock_driver());

  // There are cases, such as when a form is explicitly for creating a new
  // password, where we may ignore saved credentials. Make sure that we still
  // allow generation in that case.
  PasswordForm signup_form(*observed_form());
  signup_form.new_password_element = base::ASCIIToUTF16("new_password_field");

  PasswordFormManager manager_dropped_creds(
      password_manager(), client(), client()->driver(), signup_form, false);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);
  manager_dropped_creds.SimulateFetchMatchingLoginsFromPasswordStore();
  simulated_results.push_back(CreateSavedMatch(false));
  manager_dropped_creds.OnGetPasswordStoreResults(simulated_results.Pass());
  Mock::VerifyAndClearExpectations(client()->mock_driver());

  // Signing up on a previously visited site. Credentials are found in the
  // password store, but they are blacklisted. AllowPasswordGenerationForForm
  // should not be called and no "not blacklisted" message sent.
  PasswordFormManager manager_blacklisted(password_manager(), client(),
                                          client()->driver(), *observed_form(),
                                          false);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(0);
  manager_blacklisted.SimulateFetchMatchingLoginsFromPasswordStore();
  simulated_results.push_back(CreateSavedMatch(true));
  manager_blacklisted.OnGetPasswordStoreResults(simulated_results.Pass());
  Mock::VerifyAndClearExpectations(client()->mock_driver());
}

TEST_F(PasswordFormManagerTest, TestForceInclusionOfGeneratedPasswords) {
  // Simulate having two matches for this origin, one of which was from a form
  // with different HTML tags for elements. Because of scoring differences,
  // only the first form will be sent to Autofill().
  PasswordFormManager manager_match(password_manager(), client(),
                                    client()->driver(), *observed_form(),
                                    false);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);

  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results[1]->username_value = ASCIIToUTF16("other@gmail.com");
  simulated_results[1]->password_element = ASCIIToUTF16("signup_password");
  simulated_results[1]->username_element = ASCIIToUTF16("signup_username");
  manager_match.SimulateFetchMatchingLoginsFromPasswordStore();
  manager_match.OnGetPasswordStoreResults(simulated_results.Pass());
  EXPECT_EQ(1u, password_manager()->GetLatestBestMatches().size());

  // Same thing, except this time the credentials that don't match quite as
  // well are generated. They should now be sent to Autofill().
  PasswordFormManager manager_no_match(password_manager(), client(),
                                       client()->driver(), *observed_form(),
                                       false);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);

  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results[1]->username_value = ASCIIToUTF16("other@gmail.com");
  simulated_results[1]->password_element = ASCIIToUTF16("signup_password");
  simulated_results[1]->username_element = ASCIIToUTF16("signup_username");
  simulated_results[1]->type = PasswordForm::TYPE_GENERATED;
  manager_no_match.SimulateFetchMatchingLoginsFromPasswordStore();
  manager_no_match.OnGetPasswordStoreResults(simulated_results.Pass());
  EXPECT_EQ(2u, password_manager()->GetLatestBestMatches().size());
}

TEST_F(PasswordFormManagerTest, TestSanitizePossibleUsernames) {
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);
  PasswordForm credentials(*observed_form());
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("543-43-1234"));
  credentials.other_possible_usernames.push_back(
      ASCIIToUTF16("378282246310005"));
  credentials.other_possible_usernames.push_back(
      ASCIIToUTF16("other username"));
  credentials.username_value = ASCIIToUTF16("test@gmail.com");

  SanitizePossibleUsernames(&form_manager, &credentials);

  // Possible credit card number and SSN are stripped.
  std::vector<base::string16> expected;
  expected.push_back(ASCIIToUTF16("other username"));
  EXPECT_THAT(credentials.other_possible_usernames, Eq(expected));

  credentials.other_possible_usernames.clear();
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("511-32-9830"));
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("duplicate"));
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("duplicate"));
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("random"));
  credentials.other_possible_usernames.push_back(
      ASCIIToUTF16("test@gmail.com"));

  SanitizePossibleUsernames(&form_manager, &credentials);

  // SSN, duplicate in |other_possible_usernames| and duplicate of
  // |username_value| all removed.
  expected.clear();
  expected.push_back(ASCIIToUTF16("duplicate"));
  expected.push_back(ASCIIToUTF16("random"));
  EXPECT_THAT(credentials.other_possible_usernames, Eq(expected));
}

TEST_F(PasswordFormManagerTest, TestUpdateIncompleteCredentials) {
  // We've found this form on a website:
  PasswordForm encountered_form;
  encountered_form.origin = GURL("http://accounts.google.com/LoginAuth");
  encountered_form.signon_realm = "http://accounts.google.com/";
  encountered_form.action = GURL("http://accounts.google.com/Login");
  encountered_form.username_element = ASCIIToUTF16("Email");
  encountered_form.password_element = ASCIIToUTF16("Passwd");
  encountered_form.submit_element = ASCIIToUTF16("signIn");

  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), encountered_form, false);

  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  EXPECT_CALL(*mock_store(),
              GetLogins(encountered_form, auth_policy, &form_manager));
  form_manager.FetchMatchingLoginsFromPasswordStore(auth_policy);

  // Password store only has these incomplete credentials.
  scoped_ptr<PasswordForm> incomplete_form(new PasswordForm());
  incomplete_form->origin = GURL("http://accounts.google.com/LoginAuth");
  incomplete_form->signon_realm = "http://accounts.google.com/";
  incomplete_form->password_value = ASCIIToUTF16("my_password");
  incomplete_form->username_value = ASCIIToUTF16("my_username");
  incomplete_form->preferred = true;
  incomplete_form->ssl_valid = false;
  incomplete_form->scheme = PasswordForm::SCHEME_HTML;

  // We expect to see this form eventually sent to the Password store. It
  // has password/username values from the store and 'username_element',
  // 'password_element', 'submit_element' and 'action' fields copied from
  // the encountered form.
  PasswordForm complete_form(*incomplete_form);
  complete_form.action = encountered_form.action;
  complete_form.password_element = encountered_form.password_element;
  complete_form.username_element = encountered_form.username_element;
  complete_form.submit_element = encountered_form.submit_element;

  PasswordForm obsolete_form(*incomplete_form);
  obsolete_form.action = encountered_form.action;

  // Feed the incomplete credentials to the manager.
  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(incomplete_form.Pass());
  form_manager.OnGetPasswordStoreResults(simulated_results.Pass());

  form_manager.ProvisionallySave(
      complete_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  // By now that form has been used once.
  complete_form.times_used = 1;
  obsolete_form.times_used = 1;

  // Check that PasswordStore receives an update request with the complete form.
  EXPECT_CALL(*mock_store(),
              UpdateLoginWithPrimaryKey(complete_form, obsolete_form));
  form_manager.Save();
}

TEST_F(PasswordFormManagerTest, TestScoringPublicSuffixMatch) {
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  // Simulate having two matches for this form, first comes from different
  // signon realm, but reports the same origin and action as matched form.
  // Second candidate has the same signon realm as the form, but has a different
  // origin and action. Public suffix match is the most important criterion so
  // the second candidate should be selected.
  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results[0]->original_signon_realm = "http://accounts2.google.com";
  simulated_results[1]->origin =
      GURL("http://accounts.google.com/a/ServiceLoginAuth2");
  simulated_results[1]->action =
      GURL("http://accounts.google.com/a/ServiceLogin2");
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(simulated_results.Pass());
  EXPECT_EQ(1u, password_manager()->GetLatestBestMatches().size());
  EXPECT_EQ("", password_manager()
                    ->GetLatestBestMatches()
                    .begin()
                    ->second->original_signon_realm);
}

TEST_F(PasswordFormManagerTest, AndroidCredentialsAreAutofilled) {
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  // Although Android-based credentials are treated similarly to PSL-matched
  // credentials in most respects, they should be autofilled as opposed to be
  // filled on username-select.
  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(new PasswordForm());
  simulated_results[0]->signon_realm = observed_form()->signon_realm;
  simulated_results[0]->original_signon_realm =
      "android://hash@com.google.android";
  simulated_results[0]->origin = observed_form()->origin;
  simulated_results[0]->username_value = saved_match()->username_value;
  simulated_results[0]->password_value = saved_match()->password_value;

  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(simulated_results.Pass());

  EXPECT_EQ(1u, password_manager()->GetLatestBestMatches().size());
  EXPECT_FALSE(password_manager()->GetLatestWaitForUsername());
}

TEST_F(PasswordFormManagerTest, InvalidActionURLsDoNotMatch) {
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);

  PasswordForm invalid_action_form(*observed_form());
  invalid_action_form.action = GURL("http://");
  ASSERT_FALSE(invalid_action_form.action.is_valid());
  ASSERT_FALSE(invalid_action_form.action.is_empty());
  // Non-empty invalid action URLs should not match other actions.
  // First when the compared form has an invalid URL:
  EXPECT_EQ(0, form_manager.DoesManage(invalid_action_form) &
                   PasswordFormManager::RESULT_ACTION_MATCH);
  // Then when the observed form has an invalid URL:
  PasswordForm valid_action_form(*observed_form());
  PasswordFormManager invalid_manager(password_manager(), client(), kNoDriver,
                                      invalid_action_form, false);
  EXPECT_EQ(0,
            invalid_manager.DoesManage(valid_action_form) &
                PasswordFormManager::RESULT_ACTION_MATCH);
}

TEST_F(PasswordFormManagerTest, EmptyActionURLsDoNotMatchNonEmpty) {
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);

  PasswordForm empty_action_form(*observed_form());
  empty_action_form.action = GURL();
  ASSERT_FALSE(empty_action_form.action.is_valid());
  ASSERT_TRUE(empty_action_form.action.is_empty());
  // First when the compared form has an empty URL:
  EXPECT_EQ(0, form_manager.DoesManage(empty_action_form) &
                   PasswordFormManager::RESULT_ACTION_MATCH);
  // Then when the observed form has an empty URL:
  PasswordForm valid_action_form(*observed_form());
  PasswordFormManager empty_action_manager(password_manager(), client(),
                                           kNoDriver, empty_action_form, false);
  EXPECT_EQ(0,
            empty_action_manager.DoesManage(valid_action_form) &
                PasswordFormManager::RESULT_ACTION_MATCH);
}

TEST_F(PasswordFormManagerTest, NonHTMLFormsDoNotMatchHTMLForms) {
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);

  ASSERT_EQ(PasswordForm::SCHEME_HTML, observed_form()->scheme);
  PasswordForm non_html_form(*observed_form());
  non_html_form.scheme = PasswordForm::SCHEME_DIGEST;
  EXPECT_EQ(0, form_manager.DoesManage(non_html_form) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);

  // The other way round: observing a non-HTML form, don't match a HTML form.
  PasswordForm html_form(*observed_form());
  PasswordFormManager non_html_manager(password_manager(), client(), kNoDriver,
                                       non_html_form, false);
  EXPECT_EQ(0, non_html_manager.DoesManage(html_form) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);
}

TEST_F(PasswordFormManagerTest, OriginCheck_HostsMatchExactly) {
  // Host part of origins must match exactly, not just by prefix.
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);

  PasswordForm form_longer_host(*observed_form());
  form_longer_host.origin = GURL("http://accounts.google.com.au/a/LoginAuth");
  // Check that accounts.google.com does not match accounts.google.com.au.
  EXPECT_EQ(0, form_manager.DoesManage(form_longer_host) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);
}

TEST_F(PasswordFormManagerTest, OriginCheck_MoreSecureSchemePathsMatchPrefix) {
  // If the URL scheme of the observed form is HTTP, and the compared form is
  // HTTPS, then the compared form can extend the path.
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);

  PasswordForm form_longer_path(*observed_form());
  form_longer_path.origin = GURL("https://accounts.google.com/a/LoginAuth/sec");
  EXPECT_NE(0, form_manager.DoesManage(form_longer_path) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);
}

TEST_F(PasswordFormManagerTest,
       OriginCheck_NotMoreSecureSchemePathsMatchExactly) {
  // If the origin URL scheme of the compared form is not more secure than that
  // of the observed form, then the paths must match exactly.
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);

  PasswordForm form_longer_path(*observed_form());
  form_longer_path.origin = GURL("http://accounts.google.com/a/LoginAuth/sec");
  // Check that /a/LoginAuth does not match /a/LoginAuth/more.
  EXPECT_EQ(0, form_manager.DoesManage(form_longer_path) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);

  PasswordForm secure_observed_form(*observed_form());
  secure_observed_form.origin = GURL("https://accounts.google.com/a/LoginAuth");
  PasswordFormManager secure_manager(password_manager(), client(), kNoDriver,
                                     secure_observed_form, true);
  // Also for HTTPS in the observed form, and HTTP in the compared form, an
  // exact path match is expected.
  EXPECT_EQ(0, secure_manager.DoesManage(form_longer_path) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);
  // Not even upgrade to HTTPS in the compared form should help.
  form_longer_path.origin = GURL("https://accounts.google.com/a/LoginAuth/sec");
  EXPECT_EQ(0, secure_manager.DoesManage(form_longer_path) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);
}

TEST_F(PasswordFormManagerTest, OriginCheck_OnlyOriginsMatch) {
  // Make sure DoesManage() can distinguish when only origins match.
  PasswordFormManager form_manager(password_manager(), client(), kNoDriver,
                                   *observed_form(), false);
  PasswordForm different_html_attributes(*observed_form());
  different_html_attributes.password_element = ASCIIToUTF16("random_pass");
  different_html_attributes.username_element = ASCIIToUTF16("random_user");

  EXPECT_EQ(0, form_manager.DoesManage(different_html_attributes) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);

  EXPECT_EQ(PasswordFormManager::RESULT_ORIGINS_MATCH,
            form_manager.DoesManage(different_html_attributes) &
                PasswordFormManager::RESULT_ORIGINS_MATCH);
}

TEST_F(PasswordFormManagerTest, CorrectlyUpdatePasswordsWithSameUsername) {
  EXPECT_CALL(*client()->mock_driver(), AllowPasswordGenerationForForm(_));

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  // Add two credentials with the same username. Both should score the same
  // and be seen as candidates to autofill.
  PasswordForm first(*saved_match());
  first.action = observed_form()->action;
  first.password_value = ASCIIToUTF16("first");
  first.preferred = true;

  PasswordForm second(first);
  second.password_value = ASCIIToUTF16("second");
  second.preferred = false;

  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(first));
  result.push_back(new PasswordForm(second));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  // We always take the last credential with a particular username, regardless
  // of which ones are labeled preferred.
  EXPECT_EQ(ASCIIToUTF16("second"),
            form_manager.preferred_match()->password_value);

  PasswordForm login(*observed_form());
  login.username_value = saved_match()->username_value;
  login.password_value = ASCIIToUTF16("third");
  login.preferred = true;
  form_manager.ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(form_manager.IsNewLogin());

  PasswordForm saved_result;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(SaveArg<0>(&saved_result));
  EXPECT_CALL(*mock_store(), UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_)).Times(0);

  form_manager.Save();

  // Make sure that the password is updated appropriately.
  EXPECT_EQ(ASCIIToUTF16("third"), saved_result.password_value);
}

TEST_F(PasswordFormManagerTest, UploadFormData_NewPassword) {
  // For newly saved passwords, upload a password vote for autofill::PASSWORD.
  // Don't vote for the username field yet.
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *saved_match(), false);
  SimulateMatchingPhase(&form_manager, RESULT_NO_MATCH);

  PasswordForm form_to_save(*saved_match());
  form_to_save.preferred = true;
  form_to_save.username_value = ASCIIToUTF16("username");
  form_to_save.password_value = ASCIIToUTF16("1234");

  EXPECT_CALL(*client()->mock_driver()->mock_autofill_manager(),
              UploadPasswordForm(_, base::string16(), autofill::PASSWORD, _))
      .Times(1);
  form_manager.ProvisionallySave(
      form_to_save, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager.Save();
  Mock::VerifyAndClearExpectations(&form_manager);

  // Do not upload a vote if the user is blacklisting the form.
  PasswordFormManager blacklist_form_manager(
      password_manager(), client(), client()->driver(), *saved_match(), false);
  SimulateMatchingPhase(&blacklist_form_manager, RESULT_NO_MATCH);

  EXPECT_CALL(*client()->mock_driver()->mock_autofill_manager(),
              UploadPasswordForm(_, _, autofill::PASSWORD, _))
      .Times(0);
  blacklist_form_manager.PermanentlyBlacklist();
  Mock::VerifyAndClearExpectations(&blacklist_form_manager);
}

TEST_F(PasswordFormManagerTest, UploadPasswordForm) {
  autofill::FormData form_data;
  autofill::FormFieldData field;
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("Email");
  field.form_control_type = "text";
  form_data.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form_data.fields.push_back(field);

  // Form data is different than saved form data, account creation signal should
  // be sent.
  autofill::ServerFieldType field_type = autofill::ACCOUNT_CREATION_PASSWORD;
  AccountCreationUploadTest(form_data, 0, PasswordForm::NO_SIGNAL_SENT,
                            &field_type);

  // Non-zero times used will not upload since we only upload a positive signal
  // at most once.
  AccountCreationUploadTest(form_data, 1, PasswordForm::NO_SIGNAL_SENT,
                            nullptr);

  // Same form data as saved match and POSITIVE_SIGNAL_SENT means there should
  // be a negative autofill ping sent.
  field_type = autofill::NOT_ACCOUNT_CREATION_PASSWORD;
  AccountCreationUploadTest(saved_match()->form_data, 2,
                            PasswordForm::POSITIVE_SIGNAL_SENT, &field_type);

  // For any other GenerationUplaodStatus, no autofill upload should occur
  // if the observed form data matches the saved form data.
  AccountCreationUploadTest(saved_match()->form_data, 3,
                            PasswordForm::NO_SIGNAL_SENT, nullptr);
  AccountCreationUploadTest(saved_match()->form_data, 3,
                            PasswordForm::NEGATIVE_SIGNAL_SENT, nullptr);
}

TEST_F(PasswordFormManagerTest, CorrectlySavePasswordWithoutUsernameFields) {
  EXPECT_CALL(*client()->mock_driver(), AllowPasswordGenerationForForm(_));

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(ScopedVector<PasswordForm>());

  PasswordForm login(*observed_form());
  login.username_element.clear();
  login.password_value = ASCIIToUTF16("password");
  login.preferred = true;

  form_manager.ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_TRUE(form_manager.IsNewLogin());

  PasswordForm saved_result;
  EXPECT_CALL(*mock_store(), UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store(), UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_)).WillOnce(SaveArg<0>(&saved_result));

  form_manager.Save();

  // Make sure that the password is updated appropriately.
  EXPECT_EQ(ASCIIToUTF16("password"), saved_result.password_value);
}

TEST_F(PasswordFormManagerTest, DriverDeletedBeforeStoreDone) {
  // Test graceful handling of the following situation:
  // 1. A form appears in a frame, a PFM is created for that form.
  // 2. The PFM asks the store for credentials for this form.
  // 3. The frame (and associated driver) gets deleted.
  // 4. The PFM returns the callback with credentials.
  // This test checks implicitly that after step 4 the PFM does not attempt
  // use-after-free of the deleted driver.
  std::string example_url("http://example.com");
  scoped_ptr<PasswordForm> form(new PasswordForm);
  form->origin = GURL(example_url);
  form->signon_realm = example_url;
  form->action = GURL(example_url);
  form->username_element = ASCIIToUTF16("u");
  form->password_element = ASCIIToUTF16("p");
  form->submit_element = ASCIIToUTF16("s");

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *form, false);

  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  EXPECT_CALL(*mock_store(), GetLogins(*form, auth_policy, &form_manager));
  form_manager.FetchMatchingLoginsFromPasswordStore(auth_policy);

  // Suddenly, the frame and its driver disappear.
  client()->KillDriver();

  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(form.Pass());
  form_manager.OnGetPasswordStoreResults(simulated_results.Pass());
}

TEST_F(PasswordFormManagerTest, PreferredMatchIsUpToDate) {
  // Check that preferred_match() is always a member of best_matches().
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  EXPECT_CALL(*mock_store(),
              GetLogins(*observed_form(), auth_policy, &form_manager));
  form_manager.FetchMatchingLoginsFromPasswordStore(auth_policy);

  ScopedVector<PasswordForm> simulated_results;
  scoped_ptr<PasswordForm> form(new PasswordForm(*observed_form()));
  form->username_value = ASCIIToUTF16("username");
  form->password_value = ASCIIToUTF16("password1");
  form->preferred = false;

  scoped_ptr<PasswordForm> generated_form(new PasswordForm(*form));
  generated_form->type = PasswordForm::TYPE_GENERATED;
  generated_form->password_value = ASCIIToUTF16("password2");
  generated_form->preferred = true;

  simulated_results.push_back(generated_form.Pass());
  simulated_results.push_back(form.Pass());

  form_manager.OnGetPasswordStoreResults(simulated_results.Pass());
  EXPECT_EQ(1u, form_manager.best_matches().size());
  EXPECT_EQ(form_manager.preferred_match(),
            form_manager.best_matches().begin()->second);
  // Make sure to access all fields of preferred_match; this way if it was
  // deleted, ASAN might notice it.
  PasswordForm dummy(*form_manager.preferred_match());
}

TEST_F(PasswordFormManagerTest,
       IsIngnorableChangePasswordForm_MatchingUsernameAndPassword) {
  observed_form()->new_password_element =
      base::ASCIIToUTF16("new_password_field");

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  SimulateMatchingPhase(&form_manager, RESULT_MATCH_FOUND);

  // The user submits a password on a change-password form, which does not use
  // the "autocomplete=username" mark-up (therefore Chrome had to guess what is
  // the username), but the user-typed credentials match something already
  // stored (which confirms that the guess was right).
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_value = ASCIIToUTF16("NewPassword");

  form_manager.SetSubmittedForm(credentials);
  EXPECT_FALSE(form_manager.is_ignorable_change_password_form());
}

TEST_F(PasswordFormManagerTest,
       IsIngnorableChangePasswordForm_NotMatchingPassword) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  SimulateMatchingPhase(&form_manager, RESULT_MATCH_FOUND);

  // The user submits a password on a change-password form, which does not use
  // the "autocomplete=username" mark-up (therefore Chrome had to guess what is
  // the username), and the user-typed password do not match anything already
  // stored. There is not much confidence in the guess being right, so the
  // password should not be stored.
  saved_match()->password_value = ASCIIToUTF16("DifferentPassword");
  saved_match()->new_password_element =
      base::ASCIIToUTF16("new_password_field");
  saved_match()->new_password_value = base::ASCIIToUTF16("new_pwd");
  form_manager.SetSubmittedForm(*saved_match());
  EXPECT_TRUE(form_manager.is_ignorable_change_password_form());
}

TEST_F(PasswordFormManagerTest,
       IsIngnorableChangePasswordForm_NotMatchingUsername) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  SimulateMatchingPhase(&form_manager, RESULT_MATCH_FOUND);

  // The user submits a password on a change-password form, which does not use
  // the "autocomplete=username" mark-up (therefore Chrome had to guess what is
  // the username), and the user-typed username does not match anything already
  // stored. There is not much confidence in the guess being right, so the
  // password should not be stored.
  saved_match()->username_value = ASCIIToUTF16("DifferentUsername");
  saved_match()->new_password_element =
      base::ASCIIToUTF16("new_password_field");
  saved_match()->new_password_value = base::ASCIIToUTF16("new_pwd");
  form_manager.SetSubmittedForm(*saved_match());
  EXPECT_TRUE(form_manager.is_ignorable_change_password_form());
}

TEST_F(PasswordFormManagerTest, PasswordToSave_NoElements) {
  PasswordForm form;
  EXPECT_TRUE(PasswordFormManager::PasswordToSave(form).empty());
}

TEST_F(PasswordFormManagerTest, PasswordToSave_NoNewElement) {
  PasswordForm form;
  form.password_element = base::ASCIIToUTF16("pwd");
  base::string16 kValue = base::ASCIIToUTF16("val");
  form.password_value = kValue;
  EXPECT_EQ(kValue, PasswordFormManager::PasswordToSave(form));
}

TEST_F(PasswordFormManagerTest, PasswordToSave_NoOldElement) {
  PasswordForm form;
  form.new_password_element = base::ASCIIToUTF16("new_pwd");
  base::string16 kNewValue = base::ASCIIToUTF16("new_val");
  form.new_password_value = kNewValue;
  EXPECT_EQ(kNewValue, PasswordFormManager::PasswordToSave(form));
}

TEST_F(PasswordFormManagerTest, PasswordToSave_BothButNoNewValue) {
  PasswordForm form;
  form.password_element = base::ASCIIToUTF16("pwd");
  form.new_password_element = base::ASCIIToUTF16("new_pwd");
  base::string16 kValue = base::ASCIIToUTF16("val");
  form.password_value = kValue;
  EXPECT_EQ(kValue, PasswordFormManager::PasswordToSave(form));
}

TEST_F(PasswordFormManagerTest, PasswordToSave_NewValue) {
  PasswordForm form;
  form.password_element = base::ASCIIToUTF16("pwd");
  form.new_password_element = base::ASCIIToUTF16("new_pwd");
  form.password_value = base::ASCIIToUTF16("val");
  base::string16 kNewValue = base::ASCIIToUTF16("new_val");
  form.new_password_value = kNewValue;
  EXPECT_EQ(kNewValue, PasswordFormManager::PasswordToSave(form));
}

TEST_F(PasswordFormManagerTest, TestSuggestingPasswordChangeForms) {
  // Suggesting password on the password change form on the previously visited
  // site. Credentials are found in the password store, and are not blacklisted.
  PasswordForm observed_change_password_form = *observed_form();
  observed_change_password_form.new_password_element =
      base::ASCIIToUTF16("new_pwd");
  PasswordFormManager manager_creds(password_manager(), client(),
                                    client()->driver(),
                                    observed_change_password_form, false);
  manager_creds.SimulateFetchMatchingLoginsFromPasswordStore();
  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(CreateSavedMatch(false));
  manager_creds.OnGetPasswordStoreResults(simulated_results.Pass());
  Mock::VerifyAndClearExpectations(client()->mock_driver());

  // Check that Autofill message was sent.
  EXPECT_EQ(1u, password_manager()->GetLatestBestMatches().size());
  // Check that we suggest, not autofill.
  EXPECT_TRUE(password_manager()->GetLatestWaitForUsername());
}

TEST_F(PasswordFormManagerTest, TestUpdateMethod) {
  // Add a new password field to the test form. The PasswordFormManager should
  // save the password from this field, instead of the current password field.
  observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");

  // Given that |observed_form| was most likely a change password form, it
  // should not serve as a source for updating meta-information stored with the
  // old credentials, such as element names, as they are likely going to be
  // different between change password and login forms. To test this in depth,
  // forcibly wipe |submit_element|, which should normally trigger updating this
  // field from |observed_form| in the UpdateLogin() step as a special case. We
  // will verify in the end that this did not happen.
  saved_match()->submit_element.clear();

  client()->set_is_update_password_ui_enabled(true);
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  SimulateMatchingPhase(&form_manager, RESULT_MATCH_FOUND);

  // User submits current and new credentials to the observed form.
  PasswordForm credentials(*observed_form());
  credentials.username_element.clear();
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_value = ASCIIToUTF16("test2");
  credentials.preferred = true;
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());

  // By now, the PasswordFormManager should have promoted the new password value
  // already to be the current password, and should no longer maintain any info
  // about the new password.
  EXPECT_EQ(credentials.new_password_value,
            form_manager.pending_credentials().password_value);
  EXPECT_TRUE(form_manager.pending_credentials().new_password_element.empty());
  EXPECT_TRUE(form_manager.pending_credentials().new_password_value.empty());

  // Trigger saving to exercise some special case handling in UpdateLogin().
  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  form_manager.Update(*saved_match());
  Mock::VerifyAndClearExpectations(mock_store());

  // No meta-information should be updated, only the password.
  EXPECT_EQ(credentials.new_password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_EQ(saved_match()->submit_element, new_credentials.submit_element);
}

TEST_F(PasswordFormManagerTest, WipeStoreCopyIfOutdated_BeforeStoreCallback) {
  PasswordForm form(*saved_match());
  ASSERT_FALSE(form.password_value.empty());

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), form, false);

  // Do not notify the store observer after this GetLogins call.
  EXPECT_CALL(*mock_store(), GetLogins(_, _, _));
  form_manager.FetchMatchingLoginsFromPasswordStore(
      PasswordStore::DISALLOW_PROMPT);

  PasswordForm submitted_form(form);
  submitted_form.password_value += ASCIIToUTF16("add stuff, make it different");
  form_manager.ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);
  form_manager.WipeStoreCopyIfOutdated();
  histogram_tester.ExpectUniqueSample("PasswordManager.StoreReadyWhenWiping", 0,
                                      1);
}

TEST_F(PasswordFormManagerTest, WipeStoreCopyIfOutdated_NotOutdated) {
  PasswordForm form(*saved_match());
  form.username_value = ASCIIToUTF16("test@gmail.com");
  ASSERT_FALSE(form.password_value.empty());

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), form, false);

  // For GAIA authentication, the first two usernames are equivalent to
  // test@gmail.com, but the third is not.
  PasswordForm form_related(form);
  form_related.username_value = ASCIIToUTF16("test@googlemail.com");
  PasswordForm form_related2(form);
  form_related2.username_value = ASCIIToUTF16("test");
  PasswordForm form_unrelated(form);
  form_unrelated.username_value = ASCIIToUTF16("test.else");
  EXPECT_CALL(*mock_store(), GetLogins(_, _, _))
      .WillOnce(testing::WithArg<2>(
          InvokeConsumer(form, form_related, form_related2, form_unrelated)));
  form_manager.FetchMatchingLoginsFromPasswordStore(
      PasswordStore::DISALLOW_PROMPT);

  form_manager.ProvisionallySave(
      form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);
  form_manager.WipeStoreCopyIfOutdated();
  histogram_tester.ExpectUniqueSample("PasswordManager.StoreReadyWhenWiping", 1,
                                      1);
}

TEST_F(PasswordFormManagerTest, WipeStoreCopyIfOutdated_Outdated) {
  PasswordForm form(*saved_match());
  ASSERT_FALSE(form.password_value.empty());

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), form, false);

  // For GAIA authentication, the first two usernames are equivalent to
  // test@gmail.com, but the third is not.
  PasswordForm form_related(form);
  form_related.username_value = ASCIIToUTF16("test@googlemail.com");
  PasswordForm form_related2(form);
  form_related2.username_value = ASCIIToUTF16("test");
  PasswordForm form_unrelated(form);
  form_unrelated.username_value = ASCIIToUTF16("test.else");
  EXPECT_CALL(*mock_store(), GetLogins(_, _, _))
      .WillOnce(testing::WithArg<2>(
          InvokeConsumer(form, form_related, form_related2, form_unrelated)));
  form_manager.FetchMatchingLoginsFromPasswordStore(
      PasswordStore::DISALLOW_PROMPT);

  PasswordForm submitted_form(form);
  submitted_form.password_value += ASCIIToUTF16("add stuff, make it different");
  form_manager.ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_store(), RemoveLogin(form));
  EXPECT_CALL(*mock_store(), RemoveLogin(form_related));
  EXPECT_CALL(*mock_store(), RemoveLogin(form_related2));
  EXPECT_CALL(*mock_store(), RemoveLogin(form_unrelated)).Times(0);
  form_manager.WipeStoreCopyIfOutdated();
  histogram_tester.ExpectUniqueSample("PasswordManager.StoreReadyWhenWiping", 1,
                                      1);
}

TEST_F(PasswordFormManagerTest, RemoveNoUsernameAccounts) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  PasswordForm saved_form = *saved_match();
  saved_form.username_value.clear();
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;

  saved_form.preferred = false;
  EXPECT_CALL(*mock_store(),
              AddLogin(CheckUsername(saved_match()->username_value)));
  EXPECT_CALL(*mock_store(), RemoveLogin(saved_form));

  form_manager.ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager.Save();
}

TEST_F(PasswordFormManagerTest, NotRemovePSLNoUsernameAccounts) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  PasswordForm saved_form = *saved_match();
  saved_form.username_value.clear();
  saved_form.original_signon_realm = "www.example.org";
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;

  EXPECT_CALL(*mock_store(),
              AddLogin(CheckUsername(saved_match()->username_value)));
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);

  form_manager.ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager.Save();
}

TEST_F(PasswordFormManagerTest, NotRemoveCredentialsWithUsername) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  PasswordForm saved_form = *saved_match();
  ASSERT_FALSE(saved_form.username_value.empty());
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  base::string16 username = saved_form.username_value + ASCIIToUTF16("1");
  submitted_form.username_value = username;
  submitted_form.password_value = saved_match()->password_value;

  EXPECT_CALL(*mock_store(), AddLogin(CheckUsername(username)));
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);

  form_manager.ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager.Save();
}

TEST_F(PasswordFormManagerTest, NotRemoveCredentialsWithDiferrentPassword) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  PasswordForm saved_form = *saved_match();
  saved_form.username_value.clear();
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_form.password_value + ASCIIToUTF16("1");

  EXPECT_CALL(*mock_store(),
              AddLogin(CheckUsername(saved_match()->username_value)));
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);

  form_manager.ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager.Save();
}

TEST_F(PasswordFormManagerTest, SaveNoUsernameEvenIfWithUsernamePresent) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  PasswordForm* saved_form = saved_match();
  ASSERT_FALSE(saved_match()->username_value.empty());
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(*saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value.clear();

  EXPECT_CALL(*mock_store(), AddLogin(CheckUsername(base::string16())));
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);

  form_manager.ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager.Save();
}

TEST_F(PasswordFormManagerTest, NotRemoveOnUpdate) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  ScopedVector<PasswordForm> result;
  PasswordForm saved_form = *saved_match();
  ASSERT_FALSE(saved_form.username_value.empty());
  result.push_back(new PasswordForm(saved_form));
  saved_form.username_value.clear();
  saved_form.preferred = false;
  result.push_back(new PasswordForm(saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(result.Pass());

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_form.password_value + ASCIIToUTF16("1");

  EXPECT_CALL(*mock_store(),
              UpdateLogin(CheckUsername(saved_match()->username_value)));
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);

  form_manager.ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager.Save();
}

TEST_F(PasswordFormManagerTest, GenerationStatusChangedWithPassword) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  EXPECT_CALL(*mock_store(),
              GetLogins(*observed_form(), auth_policy, &form_manager));
  form_manager.FetchMatchingLoginsFromPasswordStore(auth_policy);

  scoped_ptr<PasswordForm> generated_form(new PasswordForm(*observed_form()));
  generated_form->type = PasswordForm::TYPE_GENERATED;
  generated_form->username_value = ASCIIToUTF16("username");
  generated_form->password_value = ASCIIToUTF16("password2");
  generated_form->preferred = true;

  PasswordForm submitted_form(*generated_form);
  submitted_form.password_value = ASCIIToUTF16("password3");

  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(generated_form.Pass());
  form_manager.OnGetPasswordStoreResults(simulated_results.Pass());

  form_manager.ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  form_manager.Save();

  EXPECT_EQ(PasswordForm::TYPE_MANUAL, new_credentials.type);
}

TEST_F(PasswordFormManagerTest, GenerationStatusNotUpdatedIfPasswordUnchanged) {
  base::HistogramTester histogram_tester;

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);

  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  EXPECT_CALL(*mock_store(),
              GetLogins(*observed_form(), auth_policy, &form_manager));
  form_manager.FetchMatchingLoginsFromPasswordStore(auth_policy);

  scoped_ptr<PasswordForm> generated_form(new PasswordForm(*observed_form()));
  generated_form->type = PasswordForm::TYPE_GENERATED;
  generated_form->username_value = ASCIIToUTF16("username");
  generated_form->password_value = ASCIIToUTF16("password2");
  generated_form->preferred = true;

  PasswordForm submitted_form(*generated_form);

  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(generated_form.Pass());
  form_manager.OnGetPasswordStoreResults(simulated_results.Pass());

  form_manager.ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  form_manager.Save();

  EXPECT_EQ(PasswordForm::TYPE_GENERATED, new_credentials.type);
  histogram_tester.ExpectBucketCount("PasswordGeneration.SubmissionEvent",
                                     metrics_util::PASSWORD_USED, 1);
}

}  // namespace password_manager
