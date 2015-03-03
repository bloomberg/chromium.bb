// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
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

void RunAllPendingTasks() {
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  run_loop.Run();
}

class MockAutofillManager : public autofill::AutofillManager {
 public:
  MockAutofillManager(autofill::AutofillDriver* driver,
                      autofill::AutofillClient* client,
                      autofill::PersonalDataManager* data_manager)
      : AutofillManager(driver, client, data_manager) {}

  MOCK_METHOD2(UploadPasswordForm,
               bool(const autofill::FormData&,
                    const autofill::ServerFieldType&));

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

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  explicit TestPasswordManagerClient(PasswordStore* password_store)
      : password_store_(password_store),
        driver_(new NiceMock<MockPasswordManagerDriver>) {
    prefs_.registry()->RegisterBooleanPref(prefs::kPasswordManagerSavingEnabled,
                                           true);
  }

  bool ShouldFilterAutofillResult(const autofill::PasswordForm& form) override {
    if (form == form_to_filter_)
      return true;
    return false;
  }

  PrefService* GetPrefs() override { return &prefs_; }
  PasswordStore* GetPasswordStore() const override { return password_store_; }

  void SetFormToFilter(const autofill::PasswordForm& form) {
    form_to_filter_ = form;
  }

  MockPasswordManagerDriver* mock_driver() { return driver_.get(); }

  base::WeakPtr<PasswordManagerDriver> driver() { return driver_->AsWeakPtr(); }

  autofill::AutofillManager* GetAutofillManagerForMainFrame() override {
    return mock_driver()->mock_autofill_manager();
  }

  void KillDriver() { driver_.reset(); }

 private:
  autofill::PasswordForm form_to_filter_;

  TestingPrefServiceSimple prefs_;
  PasswordStore* password_store_;
  scoped_ptr<MockPasswordManagerDriver> driver_;
};

class TestPasswordManager : public PasswordManager {
 public:
  explicit TestPasswordManager(TestPasswordManagerClient* client)
      : PasswordManager(client) {}

  void Autofill(password_manager::PasswordManagerDriver* driver,
                const autofill::PasswordForm& form_for_autofill,
                const autofill::PasswordFormMap& best_matches,
                const autofill::PasswordForm& preferred_match,
                bool wait_for_username) const override {
    best_matches_ = best_matches;
  }

  const autofill::PasswordFormMap& GetLatestBestMatches() {
    return best_matches_;
  }

 private:
  // Marked mutable to get around constness of Autofill().
  mutable autofill::PasswordFormMap best_matches_;
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
  }

  void TearDown() override {
    if (mock_store_.get())
      mock_store_->Shutdown();
  }

  MockPasswordStore* mock_store() const { return mock_store_.get(); }

  PasswordForm* GetPendingCredentials(PasswordFormManager* p) {
    return &p->pending_credentials_;
  }

  void SimulateMatchingPhase(PasswordFormManager* p,
                             ResultOfSimulatedMatching result) {
    // Roll up the state to mock out the matching phase.
    p->state_ = PasswordFormManager::POST_MATCHING_PHASE;
    if (result == RESULT_NO_MATCH)
      return;

    PasswordForm* match = new PasswordForm(saved_match_);
    // Heap-allocated form is owned by p.
    p->best_matches_[match->username_value] = match;
    p->preferred_match_ = match;
  }

  void SimulateFetchMatchingLoginsFromPasswordStore(
      PasswordFormManager* manager) {
    // Just need to update the internal states.
    manager->state_ = PasswordFormManager::MATCHING_PHASE;
  }

  void SanitizePossibleUsernames(PasswordFormManager* p, PasswordForm* form) {
    p->SanitizePossibleUsernames(form);
  }

  bool IgnoredResult(PasswordFormManager* p, PasswordForm* form) {
    return p->ShouldIgnoreResult(*form);
  }

  // Save saved_match() for observed_form() where |observed_form_data|,
  // |times_used|, and |status| are used to overwrite the default values for
  // observed_form(). |field_type| is the upload that we expect from saving,
  // with nullptr meaning no upload expected.
  void AccountCreationUploadTest(const autofill::FormData& observed_form_data,
                                 int times_used,
                                 PasswordForm::GenerationUploadStatus status,
                                 const autofill::ServerFieldType* field_type) {
    TestPasswordManagerClient client_with_store(mock_store());
    TestPasswordManager password_manager(&client_with_store);

    PasswordForm form(*observed_form());

    form.form_data = observed_form_data;

    PasswordFormManager form_manager(&password_manager, &client_with_store,
                                     client_with_store.driver(), form, false);
    ScopedVector<PasswordForm> result;
    result.push_back(CreateSavedMatch(false));
    result[0]->generation_upload_status = status;
    result[0]->times_used = times_used;

    PasswordForm form_to_save(form);
    form_to_save.preferred = true;
    form_to_save.username_value = result[0]->username_value;
    form_to_save.password_value = result[0]->password_value;

    SimulateFetchMatchingLoginsFromPasswordStore(&form_manager);
    form_manager.OnGetPasswordStoreResults(result.Pass());

    if (field_type) {
      EXPECT_CALL(*client_with_store.mock_driver()->mock_autofill_manager(),
                  UploadPasswordForm(_, *field_type)).Times(1);
    } else {
      EXPECT_CALL(*client_with_store.mock_driver()->mock_autofill_manager(),
                  UploadPasswordForm(_, _)).Times(0);
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

  // To spare typing for PasswordFormManager instances which need no driver.
  const base::WeakPtr<PasswordManagerDriver> kNoDriver;

 private:
  // Necessary for callbacks, and for TestAutofillDriver.
  base::MessageLoop message_loop;

  PasswordForm observed_form_;
  PasswordForm saved_match_;
  scoped_refptr<NiceMock<MockPasswordStore>> mock_store_;
  scoped_ptr<TestPasswordManagerClient> client_;
};

TEST_F(PasswordFormManagerTest, TestNewLogin) {
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);
  SimulateMatchingPhase(&manager, RESULT_NO_MATCH);

  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.preferred = true;
  manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, which should know this is a new login.
  EXPECT_TRUE(manager.IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db.
  EXPECT_EQ(observed_form()->origin.spec(),
            GetPendingCredentials(&manager)->origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            GetPendingCredentials(&manager)->signon_realm);
  EXPECT_EQ(observed_form()->action, GetPendingCredentials(&manager)->action);
  EXPECT_TRUE(GetPendingCredentials(&manager)->preferred);
  EXPECT_EQ(saved_match()->password_value,
            GetPendingCredentials(&manager)->password_value);
  EXPECT_EQ(saved_match()->username_value,
            GetPendingCredentials(&manager)->username_value);
  EXPECT_TRUE(GetPendingCredentials(&manager)->new_password_element.empty());
  EXPECT_TRUE(GetPendingCredentials(&manager)->new_password_value.empty());

  // Now, suppose the user re-visits the site and wants to save an additional
  // login for the site with a new username. In this case, the matching phase
  // will yield the previously saved login.
  SimulateMatchingPhase(&manager, RESULT_MATCH_FOUND);
  // Set up the new login.
  base::string16 new_user = ASCIIToUTF16("newuser");
  base::string16 new_pass = ASCIIToUTF16("newpass");
  credentials.username_value = new_user;
  credentials.password_value = new_pass;
  manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Again, the PasswordFormManager should know this is still a new login.
  EXPECT_TRUE(manager.IsNewLogin());
  // And make sure everything squares up again.
  EXPECT_EQ(observed_form()->origin.spec(),
            GetPendingCredentials(&manager)->origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            GetPendingCredentials(&manager)->signon_realm);
  EXPECT_TRUE(GetPendingCredentials(&manager)->preferred);
  EXPECT_EQ(new_pass, GetPendingCredentials(&manager)->password_value);
  EXPECT_EQ(new_user, GetPendingCredentials(&manager)->username_value);
  EXPECT_TRUE(GetPendingCredentials(&manager)->new_password_element.empty());
  EXPECT_TRUE(GetPendingCredentials(&manager)->new_password_value.empty());
}

// If PSL-matched credentials had been suggested, but the user has overwritten
// the password, the provisionally saved credentials should no longer be
// considered as PSL-matched, so that the exception for not prompting before
// saving PSL-matched credentials should no longer apply.
TEST_F(PasswordFormManagerTest,
       OverriddenPSLMatchedCredentialsNotMarkedAsPSLMatched) {
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  // The suggestion needs to be PSL-matched.
  saved_match()->original_signon_realm = "www.example.org";
  SimulateMatchingPhase(&manager, RESULT_MATCH_FOUND);

  // User modifies the suggested password and submits the form.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value =
      saved_match()->password_value + ASCIIToUTF16("modify");
  manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_TRUE(manager.IsNewLogin());
  EXPECT_FALSE(manager.IsPendingCredentialsPublicSuffixMatch());
}

TEST_F(PasswordFormManagerTest, PSLMatchedCredentialsMetadataUpdated) {
  TestPasswordManagerClient client_with_store(mock_store());

  TestPasswordManager manager(&client_with_store);
  PasswordFormManager form_manager(&manager, &client_with_store,
                                   client_with_store.driver(), *observed_form(),
                                   false);

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
  PasswordForm actual_saved_form;

  EXPECT_CALL(*(client_with_store.mock_driver()->mock_autofill_manager()),
              UploadPasswordForm(_, autofill::ACCOUNT_CREATION_PASSWORD))
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

  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);
  SimulateMatchingPhase(&manager, RESULT_NO_MATCH);

  // User enters current and new credentials to the observed form.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_value = ASCIIToUTF16("newpassword");
  credentials.preferred = true;
  manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, which should know this is a new login.
  EXPECT_TRUE(manager.IsNewLogin());
  EXPECT_EQ(credentials.origin, GetPendingCredentials(&manager)->origin);
  EXPECT_EQ(credentials.signon_realm,
            GetPendingCredentials(&manager)->signon_realm);
  EXPECT_EQ(credentials.action, GetPendingCredentials(&manager)->action);
  EXPECT_TRUE(GetPendingCredentials(&manager)->preferred);
  EXPECT_EQ(credentials.username_value,
            GetPendingCredentials(&manager)->username_value);

  // By this point, the PasswordFormManager should have promoted the new
  // password value to be the current password, and should have wiped the
  // password element names: they are likely going to be different on a login
  // form, so it is not worth remembering them.
  EXPECT_EQ(credentials.new_password_value,
            GetPendingCredentials(&manager)->password_value);
  EXPECT_TRUE(GetPendingCredentials(&manager)->password_element.empty());
  EXPECT_TRUE(GetPendingCredentials(&manager)->new_password_element.empty());
  EXPECT_TRUE(GetPendingCredentials(&manager)->new_password_value.empty());
}

TEST_F(PasswordFormManagerTest, TestUpdatePassword) {
  // Create a PasswordFormManager with observed_form, as if we just
  // saw this form and need to find matching logins.
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  SimulateMatchingPhase(&manager, RESULT_MATCH_FOUND);

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
  manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(manager.IsNewLogin());

  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db. (This verifies correct
  // behaviour for bug 1074420).
  EXPECT_EQ(GetPendingCredentials(&manager)->origin.spec(),
            saved_match()->origin.spec());
  EXPECT_EQ(GetPendingCredentials(&manager)->signon_realm,
            saved_match()->signon_realm);
  EXPECT_TRUE(GetPendingCredentials(&manager)->preferred);
  EXPECT_EQ(new_pass, GetPendingCredentials(&manager)->password_value);
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

  TestPasswordManagerClient client_with_store(mock_store());
  PasswordFormManager manager(nullptr, &client_with_store,
                              client_with_store.driver(), *observed_form(),
                              false);
  SimulateMatchingPhase(&manager, RESULT_MATCH_FOUND);

  // User submits current and new credentials to the observed form.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_value = ASCIIToUTF16("test2");
  credentials.preferred = true;
  manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(manager.IsNewLogin());

  // By now, the PasswordFormManager should have promoted the new password value
  // already to be the current password, and should no longer maintain any info
  // about the new password.
  EXPECT_EQ(credentials.new_password_value,
            GetPendingCredentials(&manager)->password_value);
  EXPECT_TRUE(GetPendingCredentials(&manager)->new_password_element.empty());
  EXPECT_TRUE(GetPendingCredentials(&manager)->new_password_value.empty());

  // Trigger saving to exercise some special case handling in UpdateLogin().
  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  manager.Save();
  Mock::VerifyAndClearExpectations(mock_store());

  // No meta-information should be updated, only the password.
  EXPECT_EQ(credentials.new_password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_EQ(saved_match()->submit_element, new_credentials.submit_element);
}

TEST_F(PasswordFormManagerTest, TestIgnoreResult) {
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  // Make sure we don't match a PasswordForm if it was originally saved on
  // an SSL-valid page and we are now on a page with invalid certificate.
  saved_match()->ssl_valid = true;
  EXPECT_TRUE(IgnoredResult(&manager, saved_match()));

  saved_match()->ssl_valid = false;
  // Different paths for action / origin are okay.
  saved_match()->action = GURL("http://www.google.com/b/Login");
  saved_match()->origin = GURL("http://www.google.com/foo");
  EXPECT_FALSE(IgnoredResult(&manager, saved_match()));

  // Results should be ignored if the client requests it.
  client()->SetFormToFilter(*saved_match());
  EXPECT_TRUE(IgnoredResult(&manager, saved_match()));
}

TEST_F(PasswordFormManagerTest, TestEmptyAction) {
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  saved_match()->action = GURL();
  SimulateMatchingPhase(&manager, RESULT_MATCH_FOUND);
  // User logs in with the autofilled username / password from saved_match.
  PasswordForm login = *observed_form();
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;
  manager.ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_FALSE(manager.IsNewLogin());
  // We bless our saved PasswordForm entry with the action URL of the
  // observed form.
  EXPECT_EQ(observed_form()->action, GetPendingCredentials(&manager)->action);
}

TEST_F(PasswordFormManagerTest, TestUpdateAction) {
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  SimulateMatchingPhase(&manager, RESULT_MATCH_FOUND);
  // User logs in with the autofilled username / password from saved_match.
  PasswordForm login = *observed_form();
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;

  manager.ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_FALSE(manager.IsNewLogin());
  // The observed action URL is different from the previously saved one, and
  // is the same as the one that would be submitted on successful login.
  EXPECT_NE(observed_form()->action, saved_match()->action);
  EXPECT_EQ(observed_form()->action, GetPendingCredentials(&manager)->action);
}

TEST_F(PasswordFormManagerTest, TestDynamicAction) {
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  SimulateMatchingPhase(&manager, RESULT_NO_MATCH);
  PasswordForm login(*observed_form());
  // The submitted action URL is different from the one observed on page load.
  GURL new_action = GURL("http://www.google.com/new_action");
  login.action = new_action;

  manager.ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_TRUE(manager.IsNewLogin());
  // Check that the provisionally saved action URL is the same as the submitted
  // action URL, not the one observed on page load.
  EXPECT_EQ(new_action, GetPendingCredentials(&manager)->action);
}

TEST_F(PasswordFormManagerTest, TestAlternateUsername) {
  scoped_refptr<TestPasswordStore> password_store = new TestPasswordStore;
  CHECK(password_store->Init(syncer::SyncableService::StartSyncFlare()));

  TestPasswordManagerClient client_with_store(password_store.get());
  TestPasswordManager password_manager(&client_with_store);
  PasswordFormManager manager(&password_manager, &client_with_store,
                              client_with_store.driver(), *observed_form(),
                              false);
  EXPECT_CALL(*client_with_store.mock_driver(),
              AllowPasswordGenerationForForm(_)).Times(1);

  password_store->AddLogin(*saved_match());
  manager.FetchMatchingLoginsFromPasswordStore(PasswordStore::ALLOW_PROMPT);
  RunAllPendingTasks();

  // The saved match has the right username already.
  PasswordForm login(*observed_form());
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;
  login.preferred = true;
  manager.ProvisionallySave(
      login, PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(manager.IsNewLogin());
  manager.Save();
  RunAllPendingTasks();

  // Should be only one password stored, and should not have
  // |other_possible_usernames| set anymore.
  TestPasswordStore::PasswordMap passwords = password_store->stored_passwords();
  EXPECT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[saved_match()->signon_realm].size());
  EXPECT_EQ(saved_match()->username_value,
            passwords[saved_match()->signon_realm][0].username_value);
  EXPECT_EQ(0U, passwords[saved_match()->signon_realm][0]
                    .other_possible_usernames.size());

  // This time use an alternate username
  PasswordFormManager manager_alt(&password_manager, &client_with_store,
                                  client_with_store.driver(), *observed_form(),
                                  false);
  EXPECT_CALL(*client_with_store.mock_driver(),
              AllowPasswordGenerationForForm(_)).Times(1);
  password_store->Clear();
  password_store->AddLogin(*saved_match());
  manager_alt.FetchMatchingLoginsFromPasswordStore(PasswordStore::ALLOW_PROMPT);
  RunAllPendingTasks();

  base::string16 new_username = saved_match()->other_possible_usernames[0];
  login.username_value = new_username;
  manager_alt.ProvisionallySave(
      login, PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(manager_alt.IsNewLogin());
  manager_alt.Save();
  RunAllPendingTasks();

  // |other_possible_usernames| should also be empty, but username_value should
  // be changed to match |new_username|
  passwords = password_store->stored_passwords();
  EXPECT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[saved_match()->signon_realm].size());
  EXPECT_EQ(new_username,
            passwords[saved_match()->signon_realm][0].username_value);
  EXPECT_EQ(0U, passwords[saved_match()->signon_realm][0]
                    .other_possible_usernames.size());
  password_store->Shutdown();
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
  PasswordFormManager manager1(nullptr, nullptr, kNoDriver, credentials, false);
  SimulateMatchingPhase(&manager1, RESULT_NO_MATCH);
  EXPECT_TRUE(manager1.HasValidPasswordForm());

  // Form with username_element, password_element, and new_password_element.
  PasswordFormManager manager2(nullptr, nullptr, kNoDriver, new_credentials,
                               false);
  SimulateMatchingPhase(&manager2, RESULT_NO_MATCH);
  EXPECT_TRUE(manager2.HasValidPasswordForm());

  // Form with username_element and only new_password_element.
  new_credentials.password_element.clear();
  PasswordFormManager manager3(nullptr, nullptr, kNoDriver, new_credentials,
                               false);
  SimulateMatchingPhase(&manager3, RESULT_NO_MATCH);
  EXPECT_TRUE(manager3.HasValidPasswordForm());

  // Form without a username_element but with a password_element.
  credentials.username_element.clear();
  PasswordFormManager manager4(nullptr, nullptr, kNoDriver, credentials, false);
  SimulateMatchingPhase(&manager4, RESULT_NO_MATCH);
  EXPECT_TRUE(manager4.HasValidPasswordForm());

  // Form without a username_element but with a new_password_element.
  new_credentials.username_element.clear();
  PasswordFormManager manager5(nullptr, nullptr, kNoDriver, new_credentials,
                               false);
  SimulateMatchingPhase(&manager5, RESULT_NO_MATCH);
  EXPECT_TRUE(manager5.HasValidPasswordForm());

  // Form without a password_element but with a username_element.
  credentials.username_element = saved_match()->username_element;
  credentials.password_element.clear();
  PasswordFormManager manager6(nullptr, nullptr, kNoDriver, credentials, false);
  SimulateMatchingPhase(&manager6, RESULT_NO_MATCH);
  EXPECT_FALSE(manager6.HasValidPasswordForm());

  // Form with neither a password_element nor a username_element.
  credentials.username_element.clear();
  credentials.password_element.clear();
  PasswordFormManager manager7(nullptr, nullptr, kNoDriver, credentials, false);
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
  PasswordFormManager manager1(nullptr, nullptr, kNoDriver, credentials, false);
  SimulateMatchingPhase(&manager1, RESULT_NO_MATCH);
  EXPECT_TRUE(manager1.HasValidPasswordForm());

  // Form without a username_element but with a password_element.
  credentials.username_element.clear();
  PasswordFormManager manager2(nullptr, nullptr, kNoDriver, credentials, false);
  SimulateMatchingPhase(&manager2, RESULT_NO_MATCH);
  EXPECT_TRUE(manager2.HasValidPasswordForm());

  // Form without a password_element but with a username_element.
  credentials.username_element = saved_match()->username_element;
  credentials.password_element.clear();
  PasswordFormManager manager3(nullptr, nullptr, kNoDriver, credentials, false);
  SimulateMatchingPhase(&manager3, RESULT_NO_MATCH);
  EXPECT_TRUE(manager3.HasValidPasswordForm());

  // Form with neither a password_element nor a username_element.
  credentials.username_element.clear();
  credentials.password_element.clear();
  PasswordFormManager manager4(nullptr, nullptr, kNoDriver, credentials, false);
  SimulateMatchingPhase(&manager4, RESULT_NO_MATCH);
  EXPECT_TRUE(manager4.HasValidPasswordForm());
}

TEST_F(PasswordFormManagerTest, TestSendNotBlacklistedMessage) {
  TestPasswordManager password_manager(client());
  PasswordFormManager manager_no_creds(
      &password_manager, client(), client()->driver(), *observed_form(), false);

  // First time sign-up attempt. Password store does not contain matching
  // credentials. AllowPasswordGenerationForForm should be called to send the
  // "not blacklisted" message.
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);
  SimulateFetchMatchingLoginsFromPasswordStore(&manager_no_creds);
  manager_no_creds.OnGetPasswordStoreResults(ScopedVector<PasswordForm>());
  Mock::VerifyAndClearExpectations(client()->mock_driver());

  // Signing up on a previously visited site. Credentials are found in the
  // password store, and are not blacklisted. AllowPasswordGenerationForForm
  // should be called to send the "not blacklisted" message.
  PasswordFormManager manager_creds(
      &password_manager, client(), client()->driver(), *observed_form(), false);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);
  SimulateFetchMatchingLoginsFromPasswordStore(&manager_creds);
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
      &password_manager, client(), client()->driver(), signup_form, false);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);
  SimulateFetchMatchingLoginsFromPasswordStore(&manager_dropped_creds);
  simulated_results.push_back(CreateSavedMatch(false));
  manager_dropped_creds.OnGetPasswordStoreResults(simulated_results.Pass());
  Mock::VerifyAndClearExpectations(client()->mock_driver());

  // Signing up on a previously visited site. Credentials are found in the
  // password store, but they are blacklisted. AllowPasswordGenerationForForm
  // should not be called and no "not blacklisted" message sent.
  PasswordFormManager manager_blacklisted(
      &password_manager, client(), client()->driver(), *observed_form(), false);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(0);
  SimulateFetchMatchingLoginsFromPasswordStore(&manager_blacklisted);
  simulated_results.push_back(CreateSavedMatch(true));
  manager_blacklisted.OnGetPasswordStoreResults(simulated_results.Pass());
  Mock::VerifyAndClearExpectations(client()->mock_driver());
}

TEST_F(PasswordFormManagerTest, TestForceInclusionOfGeneratedPasswords) {
  // Simulate having two matches for this origin, one of which was from a form
  // with different HTML tags for elements. Because of scoring differences,
  // only the first form will be sent to Autofill().
  TestPasswordManager password_manager(client());
  PasswordFormManager manager_match(
      &password_manager, client(), client()->driver(), *observed_form(), false);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);

  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results[1]->username_value = ASCIIToUTF16("other@gmail.com");
  simulated_results[1]->password_element = ASCIIToUTF16("signup_password");
  simulated_results[1]->username_element = ASCIIToUTF16("signup_username");
  SimulateFetchMatchingLoginsFromPasswordStore(&manager_match);
  manager_match.OnGetPasswordStoreResults(simulated_results.Pass());
  EXPECT_EQ(1u, password_manager.GetLatestBestMatches().size());

  // Same thing, except this time the credentials that don't match quite as
  // well are generated. They should now be sent to Autofill().
  PasswordFormManager manager_no_match(
      &password_manager, client(), client()->driver(), *observed_form(), false);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);

  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results[1]->username_value = ASCIIToUTF16("other@gmail.com");
  simulated_results[1]->password_element = ASCIIToUTF16("signup_password");
  simulated_results[1]->username_element = ASCIIToUTF16("signup_username");
  simulated_results[1]->type = PasswordForm::TYPE_GENERATED;
  SimulateFetchMatchingLoginsFromPasswordStore(&manager_no_match);
  manager_no_match.OnGetPasswordStoreResults(simulated_results.Pass());
  EXPECT_EQ(2u, password_manager.GetLatestBestMatches().size());
}

TEST_F(PasswordFormManagerTest, TestSanitizePossibleUsernames) {
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);
  PasswordForm credentials(*observed_form());
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("543-43-1234"));
  credentials.other_possible_usernames.push_back(
      ASCIIToUTF16("378282246310005"));
  credentials.other_possible_usernames.push_back(
      ASCIIToUTF16("other username"));
  credentials.username_value = ASCIIToUTF16("test@gmail.com");

  SanitizePossibleUsernames(&manager, &credentials);

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

  SanitizePossibleUsernames(&manager, &credentials);

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

  TestPasswordManagerClient client_with_store(mock_store());
  EXPECT_CALL(*(client_with_store.mock_driver()),
              AllowPasswordGenerationForForm(_));

  TestPasswordManager manager(&client_with_store);
  PasswordFormManager form_manager(&manager, &client_with_store,
                                   client_with_store.driver(), encountered_form,
                                   false);

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
  simulated_results.push_back(incomplete_form.release());
  form_manager.OnGetPasswordStoreResults(simulated_results.Pass());

  form_manager.ProvisionallySave(
      complete_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  // By now that form has been used once.
  complete_form.times_used = 1;
  obsolete_form.times_used = 1;

  // Check that PasswordStore receives an update request with the complete form.
  EXPECT_CALL(*mock_store(), RemoveLogin(obsolete_form));
  EXPECT_CALL(*mock_store(), AddLogin(complete_form));
  form_manager.Save();
}

TEST_F(PasswordFormManagerTest, TestScoringPublicSuffixMatch) {
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));

  TestPasswordManager password_manager(client());
  PasswordFormManager manager(&password_manager, client(), client()->driver(),
                              *observed_form(), false);

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
  SimulateFetchMatchingLoginsFromPasswordStore(&manager);
  manager.OnGetPasswordStoreResults(simulated_results.Pass());
  EXPECT_EQ(1u, password_manager.GetLatestBestMatches().size());
  EXPECT_EQ("", password_manager.GetLatestBestMatches()
                    .begin()
                    ->second->original_signon_realm);
}

TEST_F(PasswordFormManagerTest, InvalidActionURLsDoNotMatch) {
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  PasswordForm invalid_action_form(*observed_form());
  invalid_action_form.action = GURL("http://");
  ASSERT_FALSE(invalid_action_form.action.is_valid());
  ASSERT_FALSE(invalid_action_form.action.is_empty());
  // Non-empty invalid action URLs should not match other actions.
  // First when the compared form has an invalid URL:
  EXPECT_EQ(0,
            manager.DoesManage(invalid_action_form) &
                PasswordFormManager::RESULT_ACTION_MATCH);
  // Then when the observed form has an invalid URL:
  PasswordForm valid_action_form(*observed_form());
  PasswordFormManager invalid_manager(nullptr, client(), kNoDriver,
                                      invalid_action_form, false);
  EXPECT_EQ(0,
            invalid_manager.DoesManage(valid_action_form) &
                PasswordFormManager::RESULT_ACTION_MATCH);
}

TEST_F(PasswordFormManagerTest, EmptyActionURLsDoNotMatchNonEmpty) {
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  PasswordForm empty_action_form(*observed_form());
  empty_action_form.action = GURL();
  ASSERT_FALSE(empty_action_form.action.is_valid());
  ASSERT_TRUE(empty_action_form.action.is_empty());
  // First when the compared form has an empty URL:
  EXPECT_EQ(0,
            manager.DoesManage(empty_action_form) &
                PasswordFormManager::RESULT_ACTION_MATCH);
  // Then when the observed form has an empty URL:
  PasswordForm valid_action_form(*observed_form());
  PasswordFormManager empty_action_manager(nullptr, client(), kNoDriver,
                                           empty_action_form, false);
  EXPECT_EQ(0,
            empty_action_manager.DoesManage(valid_action_form) &
                PasswordFormManager::RESULT_ACTION_MATCH);
}

TEST_F(PasswordFormManagerTest, NonHTMLFormsDoNotMatchHTMLForms) {
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  ASSERT_EQ(PasswordForm::SCHEME_HTML, observed_form()->scheme);
  PasswordForm non_html_form(*observed_form());
  non_html_form.scheme = PasswordForm::SCHEME_DIGEST;
  EXPECT_EQ(0, manager.DoesManage(non_html_form) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);

  // The other way round: observing a non-HTML form, don't match a HTML form.
  PasswordForm html_form(*observed_form());
  PasswordFormManager non_html_manager(nullptr, client(), kNoDriver,
                                       non_html_form, false);
  EXPECT_EQ(0, non_html_manager.DoesManage(html_form) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);
}

TEST_F(PasswordFormManagerTest, OriginCheck_HostsMatchExactly) {
  // Host part of origins must match exactly, not just by prefix.
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  PasswordForm form_longer_host(*observed_form());
  form_longer_host.origin = GURL("http://accounts.google.com.au/a/LoginAuth");
  // Check that accounts.google.com does not match accounts.google.com.au.
  EXPECT_EQ(0, manager.DoesManage(form_longer_host) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);
}

TEST_F(PasswordFormManagerTest, OriginCheck_MoreSecureSchemePathsMatchPrefix) {
  // If the URL scheme of the observed form is HTTP, and the compared form is
  // HTTPS, then the compared form can extend the path.
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  PasswordForm form_longer_path(*observed_form());
  form_longer_path.origin = GURL("https://accounts.google.com/a/LoginAuth/sec");
  EXPECT_NE(0, manager.DoesManage(form_longer_path) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);
}

TEST_F(PasswordFormManagerTest,
       OriginCheck_NotMoreSecureSchemePathsMatchExactly) {
  // If the origin URL scheme of the compared form is not more secure than that
  // of the observed form, then the paths must match exactly.
  PasswordFormManager manager(nullptr, client(), kNoDriver, *observed_form(),
                              false);

  PasswordForm form_longer_path(*observed_form());
  form_longer_path.origin = GURL("http://accounts.google.com/a/LoginAuth/sec");
  // Check that /a/LoginAuth does not match /a/LoginAuth/more.
  EXPECT_EQ(0, manager.DoesManage(form_longer_path) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);

  PasswordForm secure_observed_form(*observed_form());
  secure_observed_form.origin = GURL("https://accounts.google.com/a/LoginAuth");
  PasswordFormManager secure_manager(nullptr, client(), kNoDriver,
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
  PasswordFormManager manager(NULL, client(), kNoDriver, *observed_form(),
                              false);
  PasswordForm different_html_attributes(*observed_form());
  different_html_attributes.password_element = ASCIIToUTF16("random_pass");
  different_html_attributes.username_element = ASCIIToUTF16("random_user");

  EXPECT_EQ(0, manager.DoesManage(different_html_attributes) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);

  EXPECT_EQ(PasswordFormManager::RESULT_ORIGINS_MATCH,
            manager.DoesManage(different_html_attributes) &
                PasswordFormManager::RESULT_ORIGINS_MATCH);
}

TEST_F(PasswordFormManagerTest, CorrectlyUpdatePasswordsWithSameUsername) {
  scoped_refptr<TestPasswordStore> password_store = new TestPasswordStore;
  CHECK(password_store->Init(syncer::SyncableService::StartSyncFlare()));

  TestPasswordManagerClient client_with_store(password_store.get());
  TestPasswordManager password_manager(&client_with_store);
  EXPECT_CALL(*client_with_store.mock_driver(),
              AllowPasswordGenerationForForm(_)).Times(2);

  // Add two credentials with the same username. Both should score the same
  // and be seen as candidates to autofill.
  PasswordForm first(*saved_match());
  first.action = observed_form()->action;
  first.password_value = ASCIIToUTF16("first");
  password_store->AddLogin(first);

  PasswordForm second(first);
  second.origin = GURL("http://accounts.google.com/a/AddLogin");
  second.password_value = ASCIIToUTF16("second");
  second.preferred = false;
  password_store->AddLogin(second);

  PasswordFormManager storing_manager(&password_manager, &client_with_store,
                                      client_with_store.driver(),
                                      *observed_form(), false);
  storing_manager.FetchMatchingLoginsFromPasswordStore(
      PasswordStore::ALLOW_PROMPT);
  RunAllPendingTasks();

  // We always take the last credential with a particular username, regardless
  // of which ones are labeled preferred.
  EXPECT_EQ(ASCIIToUTF16("second"),
            storing_manager.preferred_match()->password_value);

  PasswordForm login(*observed_form());
  login.username_value = saved_match()->username_value;
  login.password_value = ASCIIToUTF16("third");
  login.preferred = true;
  storing_manager.ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(storing_manager.IsNewLogin());
  storing_manager.Save();
  RunAllPendingTasks();

  PasswordFormManager retrieving_manager(&password_manager, &client_with_store,
                                         client_with_store.driver(),
                                         *observed_form(), false);

  retrieving_manager.FetchMatchingLoginsFromPasswordStore(
      PasswordStore::ALLOW_PROMPT);
  RunAllPendingTasks();

  // Make sure that the preferred match is updated appropriately.
  EXPECT_EQ(ASCIIToUTF16("third"),
            retrieving_manager.preferred_match()->password_value);
  password_store->Shutdown();
}

TEST_F(PasswordFormManagerTest, UploadFormData_NewPassword) {
  TestPasswordManagerClient client_with_store(mock_store());
  TestPasswordManager password_manager(&client_with_store);

  // For newly saved passwords, upload a vote for autofill::PASSWORD.
  PasswordFormManager form_manager(&password_manager, &client_with_store,
                                   client_with_store.driver(), *saved_match(),
                                   false);
  SimulateMatchingPhase(&form_manager, RESULT_NO_MATCH);

  PasswordForm form_to_save(*saved_match());
  form_to_save.preferred = true;
  form_to_save.username_value = ASCIIToUTF16("username");
  form_to_save.password_value = ASCIIToUTF16("1234");

  EXPECT_CALL(*client_with_store.mock_driver()->mock_autofill_manager(),
              UploadPasswordForm(_, autofill::PASSWORD)).Times(1);
  form_manager.ProvisionallySave(
      form_to_save, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager.Save();
  Mock::VerifyAndClearExpectations(&form_manager);

  // Do not upload a vote if the user is blacklisting the form.
  PasswordFormManager blacklist_form_manager(
      &password_manager, &client_with_store, client_with_store.driver(),
      *saved_match(), false);
  SimulateMatchingPhase(&blacklist_form_manager, RESULT_NO_MATCH);

  EXPECT_CALL(*client_with_store.mock_driver()->mock_autofill_manager(),
              UploadPasswordForm(_, autofill::PASSWORD)).Times(0);
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
  scoped_refptr<TestPasswordStore> password_store = new TestPasswordStore;
  CHECK(password_store->Init(syncer::SyncableService::StartSyncFlare()));

  TestPasswordManagerClient client_with_store(password_store.get());
  TestPasswordManager password_manager(&client_with_store);
  EXPECT_CALL(*client_with_store.mock_driver(),
              AllowPasswordGenerationForForm(_)).Times(2);

  PasswordForm form(*observed_form());
  form.username_element.clear();
  form.password_value = ASCIIToUTF16("password");
  form.preferred = true;

  PasswordFormManager storing_manager(&password_manager, &client_with_store,
                                      client_with_store.driver(),
                                      *observed_form(), false);
  storing_manager.FetchMatchingLoginsFromPasswordStore(
      PasswordStore::ALLOW_PROMPT);
  RunAllPendingTasks();

  storing_manager.ProvisionallySave(
      form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_TRUE(storing_manager.IsNewLogin());
  storing_manager.Save();
  RunAllPendingTasks();

  PasswordFormManager retrieving_manager(&password_manager, &client_with_store,
                                         client_with_store.driver(),
                                         *observed_form(), false);

  retrieving_manager.FetchMatchingLoginsFromPasswordStore(
      PasswordStore::ALLOW_PROMPT);
  RunAllPendingTasks();

  // Make sure that the preferred match is updated appropriately.
  EXPECT_EQ(ASCIIToUTF16("password"),
            retrieving_manager.preferred_match()->password_value);
  password_store->Shutdown();
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

  TestPasswordManagerClient client_with_store(mock_store());

  TestPasswordManager manager(&client_with_store);
  PasswordFormManager form_manager(&manager, &client_with_store,
                                   client_with_store.driver(), *form, false);

  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  EXPECT_CALL(*mock_store(), GetLogins(*form, auth_policy, &form_manager));
  form_manager.FetchMatchingLoginsFromPasswordStore(auth_policy);

  // Suddenly, the frame and its driver disappear.
  client_with_store.KillDriver();

  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(form.release());
  form_manager.OnGetPasswordStoreResults(simulated_results.Pass());
}

TEST_F(PasswordFormManagerTest, PreferredMatchIsUpToDate) {
  // Check that preferred_match() is always a member of best_matches().
  TestPasswordManagerClient client_with_store(mock_store());

  TestPasswordManager manager(&client_with_store);
  PasswordFormManager form_manager(&manager, &client_with_store,
                                   client_with_store.driver(), *observed_form(),
                                   false);

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

  TestPasswordManagerClient client_with_store(mock_store());
  PasswordFormManager manager(nullptr, &client_with_store,
                              client_with_store.driver(), *observed_form(),
                              false);
  SimulateMatchingPhase(&manager, RESULT_MATCH_FOUND);

  // The user submits a password on a change-password form, which does not use
  // the "autocomplete=username" mark-up (therefore Chrome had to guess what is
  // the username), but the user-typed credentials match something already
  // stored (which confirms that the guess was right).
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_value = ASCIIToUTF16("NewPassword");

  EXPECT_FALSE(manager.IsIgnorableChangePasswordForm(credentials));
}

TEST_F(PasswordFormManagerTest,
       IsIngnorableChangePasswordForm_NotMatchingPassword) {
  TestPasswordManagerClient client_with_store(mock_store());
  PasswordFormManager manager(nullptr, &client_with_store,
                              client_with_store.driver(), *observed_form(),
                              false);
  SimulateMatchingPhase(&manager, RESULT_MATCH_FOUND);

  // The user submits a password on a change-password form, which does not use
  // the "autocomplete=username" mark-up (therefore Chrome had to guess what is
  // the username), and the user-typed password do not match anything already
  // stored. There is not much confidence in the guess being right, so the
  // password should not be stored.
  saved_match()->password_value = ASCIIToUTF16("DifferentPassword");
  saved_match()->new_password_element =
      base::ASCIIToUTF16("new_password_field");
  EXPECT_TRUE(manager.IsIgnorableChangePasswordForm(*saved_match()));
}

TEST_F(PasswordFormManagerTest,
       IsIngnorableChangePasswordForm_NotMatchingUsername) {
  TestPasswordManagerClient client_with_store(mock_store());
  PasswordFormManager manager(nullptr, &client_with_store,
                              client_with_store.driver(), *observed_form(),
                              false);
  SimulateMatchingPhase(&manager, RESULT_MATCH_FOUND);

  // The user submits a password on a change-password form, which does not use
  // the "autocomplete=username" mark-up (therefore Chrome had to guess what is
  // the username), and the user-typed username does not match anything already
  // stored. There is not much confidence in the guess being right, so the
  // password should not be stored.
  saved_match()->username_value = ASCIIToUTF16("DifferentUsername");
  saved_match()->new_password_element =
      base::ASCIIToUTF16("new_password_field");
  EXPECT_TRUE(manager.IsIgnorableChangePasswordForm(*saved_match()));
}

}  // namespace password_manager
