// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <map>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAre;

namespace password_manager {

namespace {

// Invokes the password store consumer with a copy of all forms.
ACTION_P4(InvokeConsumer, form1, form2, form3, form4) {
  ScopedVector<PasswordForm> result;
  result.push_back(make_scoped_ptr(new PasswordForm(form1)));
  result.push_back(make_scoped_ptr(new PasswordForm(form2)));
  result.push_back(make_scoped_ptr(new PasswordForm(form3)));
  result.push_back(make_scoped_ptr(new PasswordForm(form4)));
  arg0->OnGetPasswordStoreResults(std::move(result));
}

MATCHER_P(CheckUsername, username_value, "Username incorrect") {
  return arg.username_value == username_value;
}

MATCHER_P2(CheckUploadFormStructure,
           form_signature,
           expected_types,
           "Upload form structure is incorrect") {
  if (form_signature != arg.FormSignature()) {
    // An unexpected form is uploaded.
    return false;
  }
  for (const autofill::AutofillField* field : arg) {
    if (expected_types.find(field->name) == expected_types.end()) {
      if (!field->possible_types().empty()) {
        // Unexpected types are uploaded.
        return false;
      }
    } else {
      if (field->possible_types().size() != 1) {
        // Currently we expect only one type per field.
        return false;
      }
      if (expected_types.find(field->name)->second !=
          *field->possible_types().begin()) {
        // An unexpected field type is found.
        return false;
      }
    }
  }
  return true;
}

void ClearVector(ScopedVector<PasswordForm>* results) {
  results->clear();
}

class MockAutofillDownloadManager : public autofill::AutofillDownloadManager {
 public:
  MockAutofillDownloadManager(
      autofill::AutofillDriver* driver,
      autofill::AutofillDownloadManager::Observer* observer)
      : AutofillDownloadManager(driver, observer) {}

  MOCK_METHOD5(StartUploadRequest,
               bool(const autofill::FormStructure&,
                    bool,
                    const autofill::ServerFieldTypeSet&,
                    const std::string&,
                    bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDownloadManager);
};

class MockAutofillManager : public autofill::AutofillManager {
 public:
  MockAutofillManager(autofill::AutofillDriver* driver,
                      autofill::AutofillClient* client,
                      autofill::PersonalDataManager* data_manager)
      : AutofillManager(driver, client, data_manager) {}

  void SetDownloadManager(autofill::AutofillDownloadManager* manager) {
    set_download_manager(manager);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillManager);
};

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver()
      : mock_autofill_manager_(&test_autofill_driver_,
                               &test_autofill_client_,
                               &test_personal_data_manager_) {
    scoped_ptr<TestingPrefServiceSimple> prefs(new TestingPrefServiceSimple());
    prefs->registry()->RegisterBooleanPref(autofill::prefs::kAutofillEnabled,
                                           true);
    test_autofill_client_.SetPrefs(std::move(prefs));
    mock_autofill_download_manager_ = new MockAutofillDownloadManager(
        &test_autofill_driver_, &mock_autofill_manager_);
    // AutofillManager takes ownership of |mock_autofill_download_manager_|.
    mock_autofill_manager_.SetDownloadManager(mock_autofill_download_manager_);
  }

  ~MockPasswordManagerDriver() {}

  MOCK_METHOD1(FillPasswordForm, void(const autofill::PasswordFormFillData&));
  MOCK_METHOD1(AllowPasswordGenerationForForm,
               void(const autofill::PasswordForm&));

  MockAutofillManager* mock_autofill_manager() {
    return &mock_autofill_manager_;
  }

  MockAutofillDownloadManager* mock_autofill_download_manager() {
    return mock_autofill_download_manager_;
  }

 private:
  autofill::TestAutofillDriver test_autofill_driver_;
  autofill::TestAutofillClient test_autofill_client_;
  autofill::TestPersonalDataManager test_personal_data_manager_;
  MockAutofillDownloadManager* mock_autofill_download_manager_;

  NiceMock<MockAutofillManager> mock_autofill_manager_;
};

class MockStoreResultFilter : public CredentialsFilter {
 public:
  MOCK_CONST_METHOD1(FilterResultsPtr,
                     void(ScopedVector<autofill::PasswordForm>* results));

  // This method is not relevant here.
  MOCK_CONST_METHOD1(ShouldSave, bool(const autofill::PasswordForm& form));

  // GMock cannot handle move-only arguments.
  ScopedVector<autofill::PasswordForm> FilterResults(
      ScopedVector<autofill::PasswordForm> results) const override {
    FilterResultsPtr(&results);
    return results;
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

  // After this method is called, the filter returned by GetStoreResultFilter()
  // will filter out all forms.
  void FilterAllResults() {
    filter_all_results_ = true;

    // EXPECT_CALL rather than ON_CALL, because if the test needs the
    // filtering, then it needs it called.
    EXPECT_CALL(all_filter_, FilterResultsPtr(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(ClearVector));
  }

  const CredentialsFilter* GetStoreResultFilter() const override {
    return filter_all_results_ ? &all_filter_ : &none_filter_;
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

  // Filters to remove all and no results, respectively, in FilterResults.
  NiceMock<MockStoreResultFilter> all_filter_;
  NiceMock<MockStoreResultFilter> none_filter_;
  bool filter_all_results_;
};

}  // namespace

class PasswordFormManagerTest : public testing::Test {
 public:
  PasswordFormManagerTest() {}

  // Types of possible outcomes of simulated matching, see
  // SimulateMatchingPhase.
  enum ResultOfSimulatedMatching {
    RESULT_NO_MATCH,
    RESULT_SAVED_MATCH = 1 << 0,  // Include saved_match_ in store results.
    RESULT_PSL_MATCH = 1 << 1,    // Include psl_saved_match_ in store results.
  };
  typedef int ResultOfSimulatedMatchingMask;

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

    psl_saved_match_ = saved_match_;
    psl_saved_match_.is_public_suffix_match = true;
    psl_saved_match_.origin =
        GURL("http://m.accounts.google.com/a/ServiceLoginAuth");
    psl_saved_match_.action = GURL("http://m.accounts.google.com/a/Login");
    psl_saved_match_.signon_realm = "http://m.accounts.google.com";

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
    field.name = ASCIIToUTF16("Passwd");
    field.form_control_type = "password";
    saved_match_.form_data.fields.push_back(field);

    mock_store_ = new NiceMock<MockPasswordStore>();
    ON_CALL(*mock_store_, GetSiteStatsMock(_))
        .WillByDefault(Return(std::vector<InteractionsStats*>()));
    client_.reset(new TestPasswordManagerClient(mock_store_.get()));
    password_manager_.reset(new PasswordManager(client_.get()));
    form_manager_.reset(new PasswordFormManager(
        password_manager_.get(), client_.get(), client_.get()->driver(),
        observed_form_, false));
  }

  void TearDown() override {
    if (mock_store_.get())
      mock_store_->ShutdownOnUIThread();
  }

  MockPasswordStore* mock_store() const { return mock_store_.get(); }

  void SimulateMatchingPhase(PasswordFormManager* p,
                             ResultOfSimulatedMatchingMask result) {
    const PasswordStore::AuthorizationPromptPolicy auth_policy =
        PasswordStore::DISALLOW_PROMPT;
    EXPECT_CALL(*mock_store(), GetLogins(p->observed_form(), auth_policy, p));
    p->FetchDataFromPasswordStore(auth_policy);
    if (result == RESULT_NO_MATCH) {
      p->OnGetPasswordStoreResults(ScopedVector<PasswordForm>());
      return;
    }

    ScopedVector<PasswordForm> result_form;
    if (result & RESULT_SAVED_MATCH) {
      result_form.push_back(new PasswordForm(saved_match_));
    }
    if (result & RESULT_PSL_MATCH) {
      result_form.push_back(new PasswordForm(psl_saved_match_));
    }
    p->OnGetPasswordStoreResults(std::move(result_form));
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
    form_manager.OnGetPasswordStoreResults(std::move(result));
    std::string expected_login_signature;
    autofill::FormStructure observed_structure(observed_form_data);
    autofill::FormStructure pending_structure(saved_match()->form_data);
    if (observed_structure.FormSignature() !=
            pending_structure.FormSignature() &&
        times_used == 0) {
      expected_login_signature = observed_structure.FormSignature();
    }
    autofill::ServerFieldTypeSet expected_available_field_types;
    expected_available_field_types.insert(autofill::USERNAME);
    std::map<base::string16, autofill::ServerFieldType> expected_types;
    expected_types[ASCIIToUTF16("full_name")] = autofill::UNKNOWN_TYPE;
    expected_types[saved_match()->username_element] =
        username_vote.empty() ? autofill::UNKNOWN_TYPE : autofill::USERNAME;
    if (field_type) {
      expected_available_field_types.insert(*field_type);
      expected_types[saved_match()->password_element] = *field_type;
    } else {
      expected_available_field_types.insert(
          autofill::NOT_ACCOUNT_CREATION_PASSWORD);
      expected_types[saved_match()->password_element] =
          autofill::NOT_ACCOUNT_CREATION_PASSWORD;
    }
    if (field_type) {
      EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                  StartUploadRequest(
                      CheckUploadFormStructure(
                          pending_structure.FormSignature(), expected_types),
                      false, expected_available_field_types,
                      expected_login_signature, true));
    } else {
      EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                  StartUploadRequest(_, _, _, _, _))
          .Times(0);
    }
    form_manager.ProvisionallySave(
        form_to_save, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
    form_manager.Save();
  }

  // Test upload votes on change password forms. |field_type| is a vote that we
  // expect to be uploaded.
  void ChangePasswordUploadTest(autofill::ServerFieldType field_type) {
    // |observed_form_| should have |form_data| in order to be uploaded.
    observed_form()->form_data = saved_match()->form_data;
    // Turn |observed_form_| and  into change password form.
    observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");
    autofill::FormFieldData field;
    field.label = ASCIIToUTF16("NewPasswd");
    field.name = ASCIIToUTF16("NewPasswd");
    field.form_control_type = "password";
    observed_form()->form_data.fields.push_back(field);

    client()->set_is_update_password_ui_enabled(true);
    PasswordFormManager form_manager(password_manager(), client(),
                                     client()->driver(), *observed_form(),
                                     false);

    SimulateMatchingPhase(&form_manager, RESULT_SAVED_MATCH);

    // User submits current and new credentials to the observed form.
    PasswordForm submitted_form(*observed_form());
    // credentials.username_element.clear();
    submitted_form.username_value = saved_match()->username_value;
    submitted_form.password_value = saved_match()->password_value;
    submitted_form.new_password_value = ASCIIToUTF16("test2");
    submitted_form.preferred = true;
    form_manager.ProvisionallySave(
        submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

    // Successful login. The PasswordManager would instruct PasswordFormManager
    // to update.
    EXPECT_FALSE(form_manager.IsNewLogin());
    EXPECT_FALSE(
        form_manager.is_possible_change_password_form_without_username());

    // By now, the PasswordFormManager should have promoted the new password
    // value already to be the current password, and should no longer maintain
    // any info about the new password value.
    EXPECT_EQ(submitted_form.new_password_value,
              form_manager.pending_credentials().password_value);
    EXPECT_TRUE(form_manager.pending_credentials().new_password_value.empty());

    std::map<base::string16, autofill::ServerFieldType> expected_types;
    expected_types[ASCIIToUTF16("full_name")] = autofill::UNKNOWN_TYPE;
    expected_types[observed_form_.username_element] = autofill::USERNAME;
    expected_types[observed_form_.password_element] = autofill::PASSWORD;
    expected_types[observed_form_.new_password_element] = field_type;

    autofill::ServerFieldTypeSet expected_available_field_types;
    expected_available_field_types.insert(autofill::USERNAME);
    expected_available_field_types.insert(autofill::PASSWORD);
    expected_available_field_types.insert(field_type);

    std::string observed_form_signature =
        autofill::FormStructure(observed_form()->form_data).FormSignature();

    std::string expected_login_signature;
    if (field_type == autofill::NEW_PASSWORD) {
      autofill::FormStructure pending_structure(saved_match()->form_data);
      expected_login_signature = pending_structure.FormSignature();
    }
    EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                StartUploadRequest(CheckUploadFormStructure(
                                       observed_form_signature, expected_types),
                                   false, expected_available_field_types,
                                   expected_login_signature, true));

    switch (field_type) {
      case autofill::NEW_PASSWORD:
        form_manager.Update(*saved_match());
        break;
      case autofill::PROBABLY_NEW_PASSWORD:
        form_manager.OnNoInteractionOnUpdate();
        break;
      case autofill::NOT_NEW_PASSWORD:
        form_manager.OnNopeUpdateClicked();
        break;
      default:
        NOTREACHED();
    }
  }

  PasswordForm* observed_form() { return &observed_form_; }
  PasswordForm* saved_match() { return &saved_match_; }
  PasswordForm* psl_saved_match() { return &psl_saved_match_; }
  PasswordForm* CreateSavedMatch(bool blacklisted) {
    // Owned by the caller of this method.
    PasswordForm* match = new PasswordForm(saved_match_);
    match->blacklisted_by_user = blacklisted;
    return match;
  }

  TestPasswordManagerClient* client() { return client_.get(); }

  PasswordManager* password_manager() { return password_manager_.get(); }

  PasswordFormManager* form_manager() { return form_manager_.get(); }

  // To spare typing for PasswordFormManager instances which need no driver.
  const base::WeakPtr<PasswordManagerDriver> kNoDriver;

 private:
  // Necessary for callbacks, and for TestAutofillDriver.
  base::MessageLoop message_loop_;

  PasswordForm observed_form_;
  PasswordForm saved_match_;
  PasswordForm psl_saved_match_;
  scoped_refptr<NiceMock<MockPasswordStore>> mock_store_;
  scoped_ptr<TestPasswordManagerClient> client_;
  scoped_ptr<PasswordManager> password_manager_;
  scoped_ptr<PasswordFormManager> form_manager_;
};

TEST_F(PasswordFormManagerTest, TestNewLogin) {
  SimulateMatchingPhase(form_manager(), RESULT_NO_MATCH);

  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.preferred = true;
  form_manager()->ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, which should know this is a new login.
  EXPECT_TRUE(form_manager()->IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db.
  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager()->pending_credentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager()->pending_credentials().signon_realm);
  EXPECT_EQ(observed_form()->action,
            form_manager()->pending_credentials().action);
  EXPECT_TRUE(form_manager()->pending_credentials().preferred);
  EXPECT_EQ(saved_match()->password_value,
            form_manager()->pending_credentials().password_value);
  EXPECT_EQ(saved_match()->username_value,
            form_manager()->pending_credentials().username_value);
  EXPECT_TRUE(
      form_manager()->pending_credentials().new_password_element.empty());
  EXPECT_TRUE(form_manager()->pending_credentials().new_password_value.empty());
}

TEST_F(PasswordFormManagerTest, TestAdditionalLogin) {
  // Now, suppose the user re-visits the site and wants to save an additional
  // login for the site with a new username. In this case, the matching phase
  // will yield the previously saved login.
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);
  // Set up the new login.
  base::string16 new_user = ASCIIToUTF16("newuser");
  base::string16 new_pass = ASCIIToUTF16("newpass");

  PasswordForm credentials = *observed_form();
  credentials.username_value = new_user;
  credentials.password_value = new_pass;
  credentials.preferred = true;

  form_manager()->ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Again, the PasswordFormManager should know this is still a new login.
  EXPECT_TRUE(form_manager()->IsNewLogin());
  // And make sure everything squares up again.
  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager()->pending_credentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager()->pending_credentials().signon_realm);
  EXPECT_TRUE(form_manager()->pending_credentials().preferred);
  EXPECT_EQ(new_pass, form_manager()->pending_credentials().password_value);
  EXPECT_EQ(new_user, form_manager()->pending_credentials().username_value);
  EXPECT_TRUE(
      form_manager()->pending_credentials().new_password_element.empty());
  EXPECT_TRUE(form_manager()->pending_credentials().new_password_value.empty());
}

TEST_F(PasswordFormManagerTest, TestBlacklist) {
  saved_match()->origin = observed_form()->origin;
  saved_match()->action = observed_form()->action;
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);
  // Set up the new login.
  PasswordForm credentials = *observed_form();
  credentials.username_value = ASCIIToUTF16("newuser");
  credentials.password_value = ASCIIToUTF16("newpass");
  form_manager()->ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_TRUE(form_manager()->IsNewLogin());
  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager()->pending_credentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager()->pending_credentials().signon_realm);

  const PasswordForm pending_form = form_manager()->pending_credentials();
  PasswordForm actual_add_form;
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_))
      .WillOnce(SaveArg<0>(&actual_add_form));
  form_manager()->PermanentlyBlacklist();
  EXPECT_EQ(pending_form, form_manager()->pending_credentials());
  EXPECT_TRUE(form_manager()->IsBlacklisted());
  EXPECT_THAT(form_manager()->blacklisted_matches(),
              ElementsAre(Pointee(actual_add_form)));
}

TEST_F(PasswordFormManagerTest, TestBlacklistMatching) {
  observed_form()->origin = GURL("http://accounts.google.com/a/LoginAuth");
  observed_form()->action = GURL("http://accounts.google.com/a/Login");
  observed_form()->signon_realm = "http://accounts.google.com";
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();

  // Doesn't match because of PSL.
  PasswordForm blacklisted_psl = *observed_form();
  blacklisted_psl.signon_realm = "http://m.accounts.google.com";
  blacklisted_psl.is_public_suffix_match = true;
  blacklisted_psl.blacklisted_by_user = true;

  // Doesn't match because of different origin.
  PasswordForm blacklisted_not_match = *observed_form();
  blacklisted_not_match.origin = GURL("http://google.com/a/LoginAuth");
  blacklisted_not_match.blacklisted_by_user = true;

  // Doesn't match because of different username element.
  PasswordForm blacklisted_not_match2 = *observed_form();
  blacklisted_not_match2.username_element = ASCIIToUTF16("Element");
  blacklisted_not_match2.blacklisted_by_user = true;

  PasswordForm blacklisted_match = *observed_form();
  blacklisted_match.origin = GURL("http://accounts.google.com/a/LoginAuth1234");
  blacklisted_match.blacklisted_by_user = true;

  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(blacklisted_psl));
  result.push_back(new PasswordForm(blacklisted_not_match));
  result.push_back(new PasswordForm(blacklisted_not_match2));
  result.push_back(new PasswordForm(blacklisted_match));
  result.push_back(new PasswordForm(*saved_match()));
  form_manager.OnGetPasswordStoreResults(std::move(result));
  EXPECT_TRUE(form_manager.IsBlacklisted());
  EXPECT_THAT(form_manager.blacklisted_matches(),
              ElementsAre(Pointee(blacklisted_match)));
  EXPECT_EQ(1u, form_manager.best_matches().size());
  EXPECT_EQ(*saved_match(), *form_manager.preferred_match());
}

TEST_F(PasswordFormManagerTest, AutofillBlacklisted) {
  // Blacklisted best matches credentials should not be autofilled, but the
  // non-blacklisted should.
  PasswordForm saved_form = *observed_form();
  saved_form.username_value = ASCIIToUTF16("user");
  saved_form.password_value = ASCIIToUTF16("pass");

  PasswordForm blacklisted = *observed_form();
  blacklisted.blacklisted_by_user = true;
  blacklisted.username_value.clear();

  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  result.push_back(new PasswordForm(blacklisted));

  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));

  form_manager()->OnGetPasswordStoreResults(std::move(result));
  EXPECT_EQ(1u, form_manager()->blacklisted_matches().size());
  EXPECT_TRUE(form_manager()->IsBlacklisted());
  EXPECT_EQ(1u, form_manager()->best_matches().size());
  EXPECT_TRUE(fill_data.additional_logins.empty());
}

// If PSL-matched credentials had been suggested, but the user has overwritten
// the password, the provisionally saved credentials should no longer be
// considered as PSL-matched, so that the exception for not prompting before
// saving PSL-matched credentials should no longer apply.
TEST_F(PasswordFormManagerTest,
       OverriddenPSLMatchedCredentialsNotMarkedAsPSLMatched) {
  // The suggestion needs to be PSL-matched.
  SimulateMatchingPhase(form_manager(), RESULT_PSL_MATCH);

  // User modifies the suggested password and submits the form.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value =
      saved_match()->password_value + ASCIIToUTF16("modify");
  form_manager()->ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_TRUE(form_manager()->IsNewLogin());
  EXPECT_FALSE(form_manager()->IsPendingCredentialsPublicSuffixMatch());
}

TEST_F(PasswordFormManagerTest, PSLMatchedCredentialsMetadataUpdated) {
  // The suggestion needs to be PSL-matched.
  saved_match()->is_public_suffix_match = true;
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;
  form_manager()->ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  PasswordForm expected_saved_form(submitted_form);
  expected_saved_form.times_used = 1;
  expected_saved_form.other_possible_usernames.clear();
  expected_saved_form.form_data = saved_match()->form_data;
  expected_saved_form.origin = observed_form()->origin;
  expected_saved_form.is_public_suffix_match = true;
  PasswordForm actual_saved_form;

  autofill::ServerFieldTypeSet expected_available_field_types;
  expected_available_field_types.insert(autofill::USERNAME);
  expected_available_field_types.insert(autofill::ACCOUNT_CREATION_PASSWORD);
  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(_, false, expected_available_field_types, _, true))
      .Times(1);
  EXPECT_CALL(*mock_store(), AddLogin(_))
      .WillOnce(SaveArg<0>(&actual_saved_form));
  form_manager()->Save();

  // Can't verify time, so ignore it.
  actual_saved_form.date_created = base::Time();
  EXPECT_EQ(expected_saved_form, actual_saved_form);
}

TEST_F(PasswordFormManagerTest, TestNewLoginFromNewPasswordElement) {
  // Add a new password field to the test form. The PasswordFormManager should
  // save the password from this field, instead of the current password field.
  observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");
  observed_form()->username_marked_by_site = true;

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
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
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);

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
  form_manager()->ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager()->IsNewLogin());

  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db. (This verifies correct
  // behaviour for bug 1074420).
  EXPECT_EQ(form_manager()->pending_credentials().origin.spec(),
            saved_match()->origin.spec());
  EXPECT_EQ(form_manager()->pending_credentials().signon_realm,
            saved_match()->signon_realm);
  EXPECT_TRUE(form_manager()->pending_credentials().preferred);
  EXPECT_EQ(new_pass, form_manager()->pending_credentials().password_value);
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
  SimulateMatchingPhase(&form_manager, RESULT_SAVED_MATCH);

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
  const bool kObservedFormSSLValid = false;
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(),
                                   kObservedFormSSLValid);

  PasswordForm saved_form = *observed_form();
  saved_form.ssl_valid = !kObservedFormSSLValid;
  saved_form.username_value = ASCIIToUTF16("test1@gmail.com");
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  saved_form.username_value = ASCIIToUTF16("test2@gmail.com");
  result.push_back(new PasswordForm(saved_form));
  saved_form.username_value = ASCIIToUTF16("test3@gmail.com");
  saved_form.ssl_valid = kObservedFormSSLValid;
  result.push_back(new PasswordForm(saved_form));
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager.OnGetPasswordStoreResults(std::move(result));

  // Make sure we don't match a PasswordForm if it was originally saved on
  // an SSL-valid page and we are now on a page with invalid certificate.
  EXPECT_EQ(1u, form_manager.best_matches().count(saved_form.username_value));
  EXPECT_EQ(1u, form_manager.best_matches().size());
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
  form_manager.OnGetPasswordStoreResults(std::move(result));

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
  form_manager.OnGetPasswordStoreResults(std::move(result));

  // Results should be ignored if the client requests it.
  EXPECT_TRUE(form_manager.best_matches().empty());
}

TEST_F(PasswordFormManagerTest, TestEmptyAction) {
  saved_match()->action = GURL();
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);
  // User logs in with the autofilled username / password from saved_match.
  PasswordForm login = *observed_form();
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;
  form_manager()->ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_FALSE(form_manager()->IsNewLogin());
  // We bless our saved PasswordForm entry with the action URL of the
  // observed form.
  EXPECT_EQ(observed_form()->action,
            form_manager()->pending_credentials().action);
}

TEST_F(PasswordFormManagerTest, TestUpdateAction) {
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);
  // User logs in with the autofilled username / password from saved_match.
  PasswordForm login = *observed_form();
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;

  form_manager()->ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_FALSE(form_manager()->IsNewLogin());
  // The observed action URL is different from the previously saved one, and
  // is the same as the one that would be submitted on successful login.
  EXPECT_NE(observed_form()->action, saved_match()->action);
  EXPECT_EQ(observed_form()->action,
            form_manager()->pending_credentials().action);
}

TEST_F(PasswordFormManagerTest, TestDynamicAction) {
  SimulateMatchingPhase(form_manager(), RESULT_NO_MATCH);
  PasswordForm login(*observed_form());
  // The submitted action URL is different from the one observed on page load.
  GURL new_action = GURL("http://www.google.com/new_action");
  login.action = new_action;

  form_manager()->ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_TRUE(form_manager()->IsNewLogin());
  // Check that the provisionally saved action URL is the same as the submitted
  // action URL, not the one observed on page load.
  EXPECT_EQ(new_action, form_manager()->pending_credentials().action);
}

TEST_F(PasswordFormManagerTest, TestAlternateUsername_NoChange) {
  EXPECT_CALL(*client()->mock_driver(), AllowPasswordGenerationForForm(_));

  PasswordForm saved_form = *saved_match();
  ASSERT_FALSE(saved_form.other_possible_usernames.empty());

  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(std::move(result));

  // The saved match has the right username already.
  PasswordForm login(*observed_form());
  login.preferred = true;
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;

  form_manager()->ProvisionallySave(
      login, PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(form_manager()->IsNewLogin());

  PasswordForm saved_result;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(SaveArg<0>(&saved_result));
  EXPECT_CALL(*mock_store(), UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_)).Times(0);

  form_manager()->Save();
  // Should be only one password stored, and should not have
  // |other_possible_usernames| set anymore.
  EXPECT_EQ(saved_match()->username_value, saved_result.username_value);
  EXPECT_TRUE(saved_result.other_possible_usernames.empty());
}

TEST_F(PasswordFormManagerTest, TestAlternateUsername_OtherUsername) {
  // This time use an alternate username.

  EXPECT_CALL(*client()->mock_driver(), AllowPasswordGenerationForForm(_));

  PasswordForm saved_form = *saved_match();
  ASSERT_FALSE(saved_form.other_possible_usernames.empty());

  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(std::move(result));

  // The saved match has the right username already.
  PasswordForm login(*observed_form());
  login.preferred = true;
  login.username_value = saved_match()->other_possible_usernames[0];
  login.password_value = saved_match()->password_value;

  form_manager()->ProvisionallySave(
      login, PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(form_manager()->IsNewLogin());

  PasswordForm saved_result;
  // Changing the username changes the primary key of the stored credential.
  EXPECT_CALL(*mock_store(), UpdateLoginWithPrimaryKey(
                                 _, CheckUsername(saved_form.username_value)))
      .WillOnce(SaveArg<0>(&saved_result));
  EXPECT_CALL(*mock_store(), UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_)).Times(0);

  form_manager()->Save();

  // |other_possible_usernames| should also be empty, but username_value should
  // be changed to match |new_username|.
  EXPECT_EQ(saved_match()->other_possible_usernames[0],
            saved_result.username_value);
  EXPECT_TRUE(saved_result.other_possible_usernames.empty());
}

TEST_F(PasswordFormManagerTest, TestSendNotBlacklistedMessage_NoCredentials) {
  // First time sign-up attempt. Password store does not contain matching
  // credentials. AllowPasswordGenerationForForm should be called to send the
  // "not blacklisted" message.
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(ScopedVector<PasswordForm>());
}

TEST_F(PasswordFormManagerTest, TestSendNotBlacklistedMessage_Credentials) {
  // Signing up on a previously visited site. Credentials are found in the
  // password store, and are not blacklisted. AllowPasswordGenerationForForm
  // should be called to send the "not blacklisted" message.
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(CreateSavedMatch(false));
  form_manager()->OnGetPasswordStoreResults(std::move(simulated_results));
}

TEST_F(PasswordFormManagerTest,
       TestSendNotBlacklistedMessage_DroppedCredentials) {
  // There are cases, such as when a form is explicitly for creating a new
  // password, where we may ignore saved credentials. Make sure that we still
  // allow generation in that case.
  PasswordForm signup_form(*observed_form());
  signup_form.new_password_element = base::ASCIIToUTF16("new_password_field");

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), signup_form, false);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(1);
  form_manager.SimulateFetchMatchingLoginsFromPasswordStore();
  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(CreateSavedMatch(false));
  form_manager.OnGetPasswordStoreResults(std::move(simulated_results));
}

TEST_F(PasswordFormManagerTest,
       TestSendNotBlacklistedMessage_BlacklistedCredentials) {
  // Signing up on a previously visited site. Credentials are found in the
  // password store, but they are blacklisted. AllowPasswordGenerationForForm
  // is still called.
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(CreateSavedMatch(true));
  form_manager()->OnGetPasswordStoreResults(std::move(simulated_results));
}

TEST_F(PasswordFormManagerTest, TestBestCredentialsByEachUsernameAreIncluded) {
  // Simulate having several matches, with 3 different usernames. Some of the
  // matches are PSL matches. One match for each username should be chosen.
  ScopedVector<PasswordForm> simulated_results;
  // Add a best scoring match. It should be in |best_matches| and chosen as a
  // prefferred match.
  simulated_results.push_back(new PasswordForm(*saved_match()));
  // Add a match saved on another form, it has lower score. It should not be in
  // |best_matches|.
  simulated_results.push_back(new PasswordForm(*saved_match()));
  simulated_results[1]->password_element = ASCIIToUTF16("signup_password");
  simulated_results[1]->username_element = ASCIIToUTF16("signup_username");
  // Add a match saved on another form with a different username. It should be
  // in |best_matches|.
  simulated_results.push_back(new PasswordForm(*saved_match()));
  auto username1 = simulated_results[0]->username_value + ASCIIToUTF16("1");
  simulated_results[2]->username_value = username1;
  simulated_results[2]->password_element = ASCIIToUTF16("signup_password");
  simulated_results[2]->username_element = ASCIIToUTF16("signup_username");
  // Add a PSL match, it should not be in |best_matches|.
  simulated_results.push_back(new PasswordForm(*psl_saved_match()));
  // Add a PSL match with a different username. It should be in |best_matches|.
  simulated_results.push_back(new PasswordForm(*psl_saved_match()));
  auto username2 = simulated_results[0]->username_value + ASCIIToUTF16("2");
  simulated_results[4]->username_value = username2;

  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));

  form_manager()->OnGetPasswordStoreResults(std::move(simulated_results));
  const autofill::PasswordFormMap& best_matches =
      form_manager()->best_matches();
  EXPECT_EQ(3u, best_matches.size());
  EXPECT_NE(best_matches.end(),
            best_matches.find(saved_match()->username_value));
  EXPECT_EQ(*saved_match(),
            *best_matches.find(saved_match()->username_value)->second);
  EXPECT_NE(best_matches.end(), best_matches.find(username1));
  EXPECT_NE(best_matches.end(), best_matches.find(username2));

  EXPECT_EQ(*saved_match(), *form_manager()->preferred_match());
  EXPECT_EQ(2u, fill_data.additional_logins.size());
}

TEST_F(PasswordFormManagerTest,
       TestForceInclusionOfGeneratedPasswords_NoMatch) {
  // Same thing, except this time the credentials that don't match quite as
  // well are generated. They should now be sent to Autofill().
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));

  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results[0]->username_value = ASCIIToUTF16("other@gmail.com");
  simulated_results[0]->password_element = ASCIIToUTF16("signup_password");
  simulated_results[0]->username_element = ASCIIToUTF16("signup_username");
  simulated_results[0]->type = PasswordForm::TYPE_GENERATED;
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));

  form_manager()->OnGetPasswordStoreResults(std::move(simulated_results));
  EXPECT_EQ(2u, form_manager()->best_matches().size());
  EXPECT_EQ(1u, fill_data.additional_logins.size());
}

TEST_F(PasswordFormManagerTest, TestSanitizePossibleUsernames) {
  const base::string16 kUsernameOther = ASCIIToUTF16("other username");

  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(ScopedVector<PasswordForm>());

  PasswordForm credentials(*observed_form());
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("543-43-1234"));
  credentials.other_possible_usernames.push_back(
      ASCIIToUTF16("378282246310005"));
  credentials.other_possible_usernames.push_back(kUsernameOther);
  credentials.username_value = ASCIIToUTF16("test@gmail.com");
  credentials.preferred = true;

  // Pass in ALLOW_OTHER_POSSIBLE_USERNAMES, although it will not make a
  // difference as no matches coming from the password store were autofilled.
  form_manager()->ProvisionallySave(
      credentials, PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES);

  PasswordForm saved_result;
  EXPECT_CALL(*mock_store(), UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store(), UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_)).WillOnce(SaveArg<0>(&saved_result));
  form_manager()->Save();

  // Possible credit card number and SSN are stripped.
  EXPECT_THAT(saved_result.other_possible_usernames,
              UnorderedElementsAre(kUsernameOther));
}

TEST_F(PasswordFormManagerTest, TestSanitizePossibleUsernamesDuplicates) {
  const base::string16 kUsernameEmail = ASCIIToUTF16("test@gmail.com");
  const base::string16 kUsernameDuplicate = ASCIIToUTF16("duplicate");
  const base::string16 kUsernameRandom = ASCIIToUTF16("random");

  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(ScopedVector<PasswordForm>());

  PasswordForm credentials(*observed_form());
  credentials.other_possible_usernames.push_back(ASCIIToUTF16("511-32-9830"));
  credentials.other_possible_usernames.push_back(kUsernameDuplicate);
  credentials.other_possible_usernames.push_back(kUsernameDuplicate);
  credentials.other_possible_usernames.push_back(kUsernameRandom);
  credentials.other_possible_usernames.push_back(kUsernameEmail);
  credentials.username_value = kUsernameEmail;
  credentials.preferred = true;

  // Pass in ALLOW_OTHER_POSSIBLE_USERNAMES, although it will not make a
  // difference as no matches coming from the password store were autofilled.
  form_manager()->ProvisionallySave(
      credentials, PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES);

  PasswordForm saved_result;
  EXPECT_CALL(*mock_store(), UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store(), UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_)).WillOnce(SaveArg<0>(&saved_result));
  form_manager()->Save();

  // SSN, duplicate in |other_possible_usernames| and duplicate of
  // |username_value| all removed.
  EXPECT_THAT(saved_result.other_possible_usernames,
              UnorderedElementsAre(kUsernameDuplicate, kUsernameRandom));
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
  form_manager.FetchDataFromPasswordStore(auth_policy);

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
  simulated_results.push_back(std::move(incomplete_form));
  form_manager.OnGetPasswordStoreResults(std::move(simulated_results));

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

  // Simulate having two matches for this form, first comes from different
  // signon realm, but reports the same origin and action as matched form.
  // Second candidate has the same signon realm as the form, but has a different
  // origin and action. Public suffix match is the most important criterion so
  // the second candidate should be selected.
  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results[0]->is_public_suffix_match = true;
  simulated_results[1]->origin =
      GURL("http://accounts.google.com/a/ServiceLoginAuth2");
  simulated_results[1]->action =
      GURL("http://accounts.google.com/a/ServiceLogin2");
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));

  form_manager()->OnGetPasswordStoreResults(std::move(simulated_results));
  EXPECT_TRUE(fill_data.additional_logins.empty());
  EXPECT_EQ(1u, form_manager()->best_matches().size());
  EXPECT_TRUE(
      !form_manager()->best_matches().begin()->second->is_public_suffix_match);
}

TEST_F(PasswordFormManagerTest, AndroidCredentialsAreAutofilled) {
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));

  // Although Android-based credentials are treated similarly to PSL-matched
  // credentials in some respects, they should be autofilled as opposed to be
  // filled on username-select.
  PasswordForm android_login;
  android_login.signon_realm = "android://hash@com.google.android";
  android_login.origin = GURL("android://hash@com.google.android/");
  android_login.is_affiliation_based_match = true;
  android_login.username_value = saved_match()->username_value;
  android_login.password_value = saved_match()->password_value;
  android_login.preferred = false;
  android_login.times_used = 42;

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));

  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(new PasswordForm(android_login));
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(std::move(simulated_results));
  EXPECT_TRUE(fill_data.additional_logins.empty());
  EXPECT_FALSE(fill_data.wait_for_username);
  EXPECT_EQ(1u, form_manager()->best_matches().size());

  // When the user submits the filled form, no copy of the credential should be
  // created, instead the usage counter of the original credential should be
  // incremented in-place, as if it were a regular credential for that website.
  PasswordForm credential(*observed_form());
  credential.username_value = android_login.username_value;
  credential.password_value = android_login.password_value;
  credential.preferred = true;
  form_manager()->ProvisionallySave(
      credential, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  EXPECT_FALSE(form_manager()->IsNewLogin());

  PasswordForm updated_credential;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(testing::SaveArg<0>(&updated_credential));
  EXPECT_CALL(*mock_store(), UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_)).Times(0);
  form_manager()->Save();
  Mock::VerifyAndClearExpectations(mock_store());

  EXPECT_EQ(android_login.username_value, updated_credential.username_value);
  EXPECT_EQ(android_login.password_value, updated_credential.password_value);
  EXPECT_EQ(android_login.times_used + 1, updated_credential.times_used);
  EXPECT_TRUE(updated_credential.preferred);
  EXPECT_EQ(GURL(), updated_credential.action);
  EXPECT_EQ(base::string16(), updated_credential.username_element);
  EXPECT_EQ(base::string16(), updated_credential.password_element);
  EXPECT_EQ(base::string16(), updated_credential.submit_element);
}

// Credentials saved through Android apps should always be shown in the drop-
// down menu, unless there is a better-scoring match with the same username.
TEST_F(PasswordFormManagerTest, AndroidCredentialsAreProtected) {
  const char kTestUsername1[] = "test-user@gmail.com";
  const char kTestUsername2[] = "test-other-user@gmail.com";
  const char kTestWebPassword[] = "web-password";
  const char kTestAndroidPassword1[] = "android-password-alpha";
  const char kTestAndroidPassword2[] = "android-password-beta";

  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));

  // Suppose there is one login saved through the website, and two other coming
  // from Android: the first has the same username as the web-based credential,
  // so it should be suppressed, but the second has a different username, so it
  // should be shown.
  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(CreateSavedMatch(false));
  simulated_results[0]->username_value = ASCIIToUTF16(kTestUsername1);
  simulated_results[0]->password_value = ASCIIToUTF16(kTestWebPassword);
  simulated_results.push_back(new PasswordForm);
  simulated_results[1]->signon_realm = "android://hash@com.google.android";
  simulated_results[1]->origin = GURL("android://hash@com.google.android/");
  simulated_results[1]->username_value = ASCIIToUTF16(kTestUsername1);
  simulated_results[1]->password_value = ASCIIToUTF16(kTestAndroidPassword1);
  simulated_results.push_back(new PasswordForm(*simulated_results[1]));
  simulated_results[2]->username_value = ASCIIToUTF16(kTestUsername2);
  simulated_results[2]->password_value = ASCIIToUTF16(kTestAndroidPassword2);

  ScopedVector<PasswordForm> expected_matches;
  expected_matches.push_back(new PasswordForm(*simulated_results[0]));
  expected_matches.push_back(new PasswordForm(*simulated_results[2]));

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(std::move(simulated_results));

  EXPECT_FALSE(fill_data.wait_for_username);
  EXPECT_EQ(1u, fill_data.additional_logins.size());

  std::vector<PasswordForm*> actual_matches;
  for (const auto& username_match_pair : form_manager()->best_matches())
    actual_matches.push_back(username_match_pair.second.get());
  EXPECT_THAT(actual_matches,
              UnorderedPasswordFormElementsAre(expected_matches.get()));
}

TEST_F(PasswordFormManagerTest, InvalidActionURLsDoNotMatch) {
  PasswordForm invalid_action_form(*observed_form());
  invalid_action_form.action = GURL("http://");
  ASSERT_FALSE(invalid_action_form.action.is_valid());
  ASSERT_FALSE(invalid_action_form.action.is_empty());
  // Non-empty invalid action URLs should not match other actions.
  // First when the compared form has an invalid URL:
  EXPECT_EQ(0, form_manager()->DoesManage(invalid_action_form) &
                   PasswordFormManager::RESULT_ACTION_MATCH);
  // Then when the observed form has an invalid URL:
  PasswordForm valid_action_form(*observed_form());
  PasswordFormManager invalid_manager(password_manager(), client(),
                                      client()->driver(), invalid_action_form,
                                      false);
  EXPECT_EQ(0,
            invalid_manager.DoesManage(valid_action_form) &
                PasswordFormManager::RESULT_ACTION_MATCH);
}

TEST_F(PasswordFormManagerTest, EmptyActionURLsDoNotMatchNonEmpty) {
  PasswordForm empty_action_form(*observed_form());
  empty_action_form.action = GURL();
  ASSERT_FALSE(empty_action_form.action.is_valid());
  ASSERT_TRUE(empty_action_form.action.is_empty());
  // First when the compared form has an empty URL:
  EXPECT_EQ(0, form_manager()->DoesManage(empty_action_form) &
                   PasswordFormManager::RESULT_ACTION_MATCH);
  // Then when the observed form has an empty URL:
  PasswordForm valid_action_form(*observed_form());
  PasswordFormManager empty_action_manager(password_manager(), client(),
                                           client()->driver(),
                                           empty_action_form, false);
  EXPECT_EQ(0,
            empty_action_manager.DoesManage(valid_action_form) &
                PasswordFormManager::RESULT_ACTION_MATCH);
}

TEST_F(PasswordFormManagerTest, NonHTMLFormsDoNotMatchHTMLForms) {
  ASSERT_EQ(PasswordForm::SCHEME_HTML, observed_form()->scheme);
  PasswordForm non_html_form(*observed_form());
  non_html_form.scheme = PasswordForm::SCHEME_DIGEST;
  EXPECT_EQ(0, form_manager()->DoesManage(non_html_form) &
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
  PasswordForm form_longer_host(*observed_form());
  form_longer_host.origin = GURL("http://accounts.google.com.au/a/LoginAuth");
  // Check that accounts.google.com does not match accounts.google.com.au.
  EXPECT_EQ(0, form_manager()->DoesManage(form_longer_host) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);
}

TEST_F(PasswordFormManagerTest, OriginCheck_MoreSecureSchemePathsMatchPrefix) {
  // If the URL scheme of the observed form is HTTP, and the compared form is
  // HTTPS, then the compared form can extend the path.
  PasswordForm form_longer_path(*observed_form());
  form_longer_path.origin = GURL("https://accounts.google.com/a/LoginAuth/sec");
  EXPECT_NE(0, form_manager()->DoesManage(form_longer_path) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);
}

TEST_F(PasswordFormManagerTest,
       OriginCheck_NotMoreSecureSchemePathsMatchExactly) {
  // If the origin URL scheme of the compared form is not more secure than that
  // of the observed form, then the paths must match exactly.
  PasswordForm form_longer_path(*observed_form());
  form_longer_path.origin = GURL("http://accounts.google.com/a/LoginAuth/sec");
  // Check that /a/LoginAuth does not match /a/LoginAuth/more.
  EXPECT_EQ(0, form_manager()->DoesManage(form_longer_path) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);

  PasswordForm secure_observed_form(*observed_form());
  secure_observed_form.origin = GURL("https://accounts.google.com/a/LoginAuth");
  PasswordFormManager secure_manager(password_manager(), client(),
                                     client()->driver(), secure_observed_form,
                                     true);
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

  PasswordForm different_html_attributes(*observed_form());
  different_html_attributes.password_element = ASCIIToUTF16("random_pass");
  different_html_attributes.username_element = ASCIIToUTF16("random_user");

  EXPECT_EQ(0, form_manager()->DoesManage(different_html_attributes) &
                   PasswordFormManager::RESULT_HTML_ATTRIBUTES_MATCH);

  EXPECT_EQ(PasswordFormManager::RESULT_ORIGINS_MATCH,
            form_manager()->DoesManage(different_html_attributes) &
                PasswordFormManager::RESULT_ORIGINS_MATCH);
}

TEST_F(PasswordFormManagerTest, CorrectlyUpdatePasswordsWithSameUsername) {
  EXPECT_CALL(*client()->mock_driver(), AllowPasswordGenerationForForm(_));

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
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(std::move(result));

  // We always take the first credential with a particular username, regardless
  // of which ones are labeled preferred.
  EXPECT_EQ(ASCIIToUTF16("first"),
            form_manager()->preferred_match()->password_value);

  PasswordForm login(*observed_form());
  login.username_value = saved_match()->username_value;
  login.password_value = ASCIIToUTF16("third");
  login.preferred = true;
  form_manager()->ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(form_manager()->IsNewLogin());

  PasswordForm saved_result;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(SaveArg<0>(&saved_result));
  EXPECT_CALL(*mock_store(), UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_)).Times(0);

  form_manager()->Save();

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

  autofill::ServerFieldTypeSet expected_available_field_types;
  expected_available_field_types.insert(autofill::USERNAME);
  expected_available_field_types.insert(autofill::PASSWORD);
  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(_, false, expected_available_field_types, _, true));
  form_manager.ProvisionallySave(
      form_to_save, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager.Save();
  Mock::VerifyAndClearExpectations(&form_manager);

  // Do not upload a vote if the user is blacklisting the form.
  PasswordFormManager blacklist_form_manager(
      password_manager(), client(), client()->driver(), *saved_match(), false);
  SimulateMatchingPhase(&blacklist_form_manager, RESULT_NO_MATCH);

  expected_available_field_types.clear();
  expected_available_field_types.insert(autofill::USERNAME);
  expected_available_field_types.insert(autofill::PASSWORD);
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, _, expected_available_field_types, _, true))
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

  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(ScopedVector<PasswordForm>());

  PasswordForm login(*observed_form());
  login.username_element.clear();
  login.password_value = ASCIIToUTF16("password");
  login.preferred = true;

  form_manager()->ProvisionallySave(
      login, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_TRUE(form_manager()->IsNewLogin());

  PasswordForm saved_result;
  EXPECT_CALL(*mock_store(), UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store(), UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_)).WillOnce(SaveArg<0>(&saved_result));

  form_manager()->Save();

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
  form_manager.FetchDataFromPasswordStore(auth_policy);

  // Suddenly, the frame and its driver disappear.
  client()->KillDriver();

  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(std::move(form));
  form_manager.OnGetPasswordStoreResults(std::move(simulated_results));
}

TEST_F(PasswordFormManagerTest, PreferredMatchIsUpToDate) {
  // Check that preferred_match() is always a member of best_matches().
  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  EXPECT_CALL(*mock_store(),
              GetLogins(*observed_form(), auth_policy, form_manager()));
  form_manager()->FetchDataFromPasswordStore(auth_policy);

  ScopedVector<PasswordForm> simulated_results;
  scoped_ptr<PasswordForm> form(new PasswordForm(*observed_form()));
  form->username_value = ASCIIToUTF16("username");
  form->password_value = ASCIIToUTF16("password1");
  form->preferred = false;

  scoped_ptr<PasswordForm> generated_form(new PasswordForm(*form));
  generated_form->type = PasswordForm::TYPE_GENERATED;
  generated_form->password_value = ASCIIToUTF16("password2");
  generated_form->preferred = true;

  simulated_results.push_back(std::move(generated_form));
  simulated_results.push_back(std::move(form));

  form_manager()->OnGetPasswordStoreResults(std::move(simulated_results));
  EXPECT_EQ(1u, form_manager()->best_matches().size());
  EXPECT_EQ(form_manager()->preferred_match(),
            form_manager()->best_matches().begin()->second.get());
  // Make sure to access all fields of preferred_match; this way if it was
  // deleted, ASAN might notice it.
  PasswordForm dummy(*form_manager()->preferred_match());
}

TEST_F(PasswordFormManagerTest,
       IsIngnorableChangePasswordForm_MatchingUsernameAndPassword) {
  observed_form()->new_password_element =
      base::ASCIIToUTF16("new_password_field");

  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  SimulateMatchingPhase(&form_manager, RESULT_SAVED_MATCH);

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
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);

  // The user submits a password on a change-password form, which does not use
  // the "autocomplete=username" mark-up (therefore Chrome had to guess what is
  // the username), and the user-typed password do not match anything already
  // stored. There is not much confidence in the guess being right, so the
  // password should not be stored.
  saved_match()->password_value = ASCIIToUTF16("DifferentPassword");
  saved_match()->new_password_element =
      base::ASCIIToUTF16("new_password_field");
  saved_match()->new_password_value = base::ASCIIToUTF16("new_pwd");
  form_manager()->SetSubmittedForm(*saved_match());
  EXPECT_TRUE(form_manager()->is_ignorable_change_password_form());
}

TEST_F(PasswordFormManagerTest,
       IsIngnorableChangePasswordForm_NotMatchingUsername) {
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);

  // The user submits a password on a change-password form, which does not use
  // the "autocomplete=username" mark-up (therefore Chrome had to guess what is
  // the username), and the user-typed username does not match anything already
  // stored. There is not much confidence in the guess being right, so the
  // password should not be stored.
  saved_match()->username_value = ASCIIToUTF16("DifferentUsername");
  saved_match()->new_password_element =
      base::ASCIIToUTF16("new_password_field");
  saved_match()->new_password_value = base::ASCIIToUTF16("new_pwd");
  form_manager()->SetSubmittedForm(*saved_match());
  EXPECT_TRUE(form_manager()->is_ignorable_change_password_form());
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

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));

  manager_creds.OnGetPasswordStoreResults(std::move(simulated_results));
  EXPECT_EQ(1u, manager_creds.best_matches().size());
  EXPECT_EQ(0u, fill_data.additional_logins.size());
  EXPECT_TRUE(fill_data.wait_for_username);
}

TEST_F(PasswordFormManagerTest, TestUpdateMethod) {
  // Add a new password field to the test form. The PasswordFormManager should
  // save the password from this field, instead of the current password field.
  observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");
  autofill::FormFieldData field;
  field.label = ASCIIToUTF16("NewPasswd");
  field.name = ASCIIToUTF16("NewPasswd");
  field.form_control_type = "password";
  observed_form()->form_data.fields.push_back(field);

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

  SimulateMatchingPhase(&form_manager, RESULT_SAVED_MATCH);
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
  EXPECT_FALSE(
      form_manager.is_possible_change_password_form_without_username());

  // By now, the PasswordFormManager should have promoted the new password value
  // already to be the current password, and should no longer maintain any info
  // about the new password value.
  EXPECT_EQ(credentials.new_password_value,
            form_manager.pending_credentials().password_value);
  EXPECT_TRUE(form_manager.pending_credentials().new_password_value.empty());

  // Trigger saving to exercise some special case handling in UpdateLogin().
  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(SaveArg<0>(&new_credentials));

  form_manager.Update(*saved_match());
  Mock::VerifyAndClearExpectations(mock_store());

  // No meta-information should be updated, only the password.
  EXPECT_EQ(credentials.new_password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_EQ(saved_match()->submit_element, new_credentials.submit_element);
}

TEST_F(PasswordFormManagerTest, TestUpdateNoUsernameTextfieldPresent) {
  // Add a new password field to the test form. The PasswordFormManager should
  // save the password from this field, instead of the current password field.
  observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");
  autofill::FormFieldData field;
  field.label = ASCIIToUTF16("NewPasswd");
  field.name = ASCIIToUTF16("NewPasswd");
  field.form_control_type = "password";
  observed_form()->form_data.fields.push_back(field);

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

  SimulateMatchingPhase(&form_manager, RESULT_SAVED_MATCH);
  // User submits current and new credentials to the observed form.
  PasswordForm credentials(*observed_form());
  // The |username_value| contains a text that's unlikely to be real username.
  credentials.username_value = ASCIIToUTF16("3");
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_value = ASCIIToUTF16("test2");
  credentials.preferred = true;
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());
  EXPECT_TRUE(form_manager.is_possible_change_password_form_without_username());

  // By now, the PasswordFormManager should have promoted the new password value
  // already to be the current password, and should no longer maintain any info
  // about the new password value.
  EXPECT_EQ(saved_match()->username_value,
            form_manager.pending_credentials().username_value);
  EXPECT_EQ(credentials.new_password_value,
            form_manager.pending_credentials().password_value);
  EXPECT_TRUE(form_manager.pending_credentials().new_password_value.empty());

  // Trigger saving to exercise some special case handling in UpdateLogin().
  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(SaveArg<0>(&new_credentials));

  form_manager.Update(form_manager.pending_credentials());
  Mock::VerifyAndClearExpectations(mock_store());

  // No meta-information should be updated, only the password.
  EXPECT_EQ(saved_match()->username_value, new_credentials.username_value);
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
  form_manager.FetchDataFromPasswordStore(PasswordStore::DISALLOW_PROMPT);

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
  form_manager.FetchDataFromPasswordStore(PasswordStore::DISALLOW_PROMPT);

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
  form_manager.FetchDataFromPasswordStore(PasswordStore::DISALLOW_PROMPT);

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
  PasswordForm saved_form = *saved_match();
  saved_form.username_value.clear();
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(std::move(result));

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;

  saved_form.preferred = false;
  EXPECT_CALL(*mock_store(),
              AddLogin(CheckUsername(saved_match()->username_value)));
  EXPECT_CALL(*mock_store(), RemoveLogin(saved_form));

  form_manager()->ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager()->Save();
}

TEST_F(PasswordFormManagerTest, NotRemovePSLNoUsernameAccounts) {
  PasswordForm saved_form = *saved_match();
  saved_form.username_value.clear();
  saved_form.is_public_suffix_match = true;
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(std::move(result));

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;

  EXPECT_CALL(*mock_store(),
              AddLogin(CheckUsername(saved_match()->username_value)));
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);

  form_manager()->ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager()->Save();
}

TEST_F(PasswordFormManagerTest, NotRemoveCredentialsWithUsername) {
  PasswordForm saved_form = *saved_match();
  ASSERT_FALSE(saved_form.username_value.empty());
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(std::move(result));

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  base::string16 username = saved_form.username_value + ASCIIToUTF16("1");
  submitted_form.username_value = username;
  submitted_form.password_value = saved_match()->password_value;

  EXPECT_CALL(*mock_store(), AddLogin(CheckUsername(username)));
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);

  form_manager()->ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager()->Save();
}

TEST_F(PasswordFormManagerTest, NotRemoveCredentialsWithDiferrentPassword) {
  PasswordForm saved_form = *saved_match();
  saved_form.username_value.clear();
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(saved_form));
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(std::move(result));

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_form.password_value + ASCIIToUTF16("1");

  EXPECT_CALL(*mock_store(),
              AddLogin(CheckUsername(saved_match()->username_value)));
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);

  form_manager()->ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager()->Save();
}

TEST_F(PasswordFormManagerTest, SaveNoUsernameEvenIfWithUsernamePresent) {
  PasswordForm* saved_form = saved_match();
  ASSERT_FALSE(saved_match()->username_value.empty());
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(*saved_form));
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(std::move(result));

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value.clear();

  EXPECT_CALL(*mock_store(), AddLogin(CheckUsername(base::string16())));
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);

  form_manager()->ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager()->Save();
}

TEST_F(PasswordFormManagerTest, NotRemoveOnUpdate) {
  ScopedVector<PasswordForm> result;
  PasswordForm saved_form = *saved_match();
  ASSERT_FALSE(saved_form.username_value.empty());
  result.push_back(new PasswordForm(saved_form));
  saved_form.username_value.clear();
  saved_form.preferred = false;
  result.push_back(new PasswordForm(saved_form));
  form_manager()->SimulateFetchMatchingLoginsFromPasswordStore();
  form_manager()->OnGetPasswordStoreResults(std::move(result));

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_form.password_value + ASCIIToUTF16("1");

  EXPECT_CALL(*mock_store(),
              UpdateLogin(CheckUsername(saved_match()->username_value)));
  EXPECT_CALL(*mock_store(), RemoveLogin(_)).Times(0);

  form_manager()->ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  form_manager()->Save();
}

TEST_F(PasswordFormManagerTest, GenerationStatusChangedWithPassword) {
  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  EXPECT_CALL(*mock_store(),
              GetLogins(*observed_form(), auth_policy, form_manager()));
  form_manager()->FetchDataFromPasswordStore(auth_policy);

  scoped_ptr<PasswordForm> generated_form(new PasswordForm(*observed_form()));
  generated_form->type = PasswordForm::TYPE_GENERATED;
  generated_form->username_value = ASCIIToUTF16("username");
  generated_form->password_value = ASCIIToUTF16("password2");
  generated_form->preferred = true;

  PasswordForm submitted_form(*generated_form);
  submitted_form.password_value = ASCIIToUTF16("password3");

  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(std::move(generated_form));
  form_manager()->OnGetPasswordStoreResults(std::move(simulated_results));

  form_manager()->ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  form_manager()->Save();

  EXPECT_EQ(PasswordForm::TYPE_MANUAL, new_credentials.type);
}

TEST_F(PasswordFormManagerTest, GenerationStatusNotUpdatedIfPasswordUnchanged) {
  base::HistogramTester histogram_tester;

  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  EXPECT_CALL(*mock_store(),
              GetLogins(*observed_form(), auth_policy, form_manager()));
  form_manager()->FetchDataFromPasswordStore(auth_policy);

  scoped_ptr<PasswordForm> generated_form(new PasswordForm(*observed_form()));
  generated_form->type = PasswordForm::TYPE_GENERATED;
  generated_form->username_value = ASCIIToUTF16("username");
  generated_form->password_value = ASCIIToUTF16("password2");
  generated_form->preferred = true;

  PasswordForm submitted_form(*generated_form);

  ScopedVector<PasswordForm> simulated_results;
  simulated_results.push_back(std::move(generated_form));
  form_manager()->OnGetPasswordStoreResults(std::move(simulated_results));

  form_manager()->ProvisionallySave(
      submitted_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  form_manager()->Save();

  EXPECT_EQ(PasswordForm::TYPE_GENERATED, new_credentials.type);
  histogram_tester.ExpectBucketCount("PasswordGeneration.SubmissionEvent",
                                     metrics_util::PASSWORD_USED, 1);
}

TEST_F(PasswordFormManagerTest,
       FetchMatchingLoginsFromPasswordStore_Reentrance) {
  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  EXPECT_CALL(*mock_store(), GetLogins(form_manager()->observed_form(),
                                       auth_policy, form_manager()))
      .Times(2);
  form_manager()->FetchDataFromPasswordStore(auth_policy);
  form_manager()->FetchDataFromPasswordStore(auth_policy);

  // First response from the store, should be ignored.
  scoped_ptr<PasswordForm> saved_form(new PasswordForm(*saved_match()));
  saved_form->username_value = ASCIIToUTF16("a@gmail.com");
  ScopedVector<PasswordForm> results;
  results.push_back(std::move(saved_form));
  form_manager()->OnGetPasswordStoreResults(std::move(results));
  EXPECT_TRUE(form_manager()->best_matches().empty());

  // Second response from the store should not be ignored.
  saved_form.reset(new PasswordForm(*saved_match()));
  saved_form->username_value = ASCIIToUTF16("b@gmail.com");
  results.push_back(std::move(saved_form));
  saved_form.reset(new PasswordForm(*saved_match()));
  saved_form->username_value = ASCIIToUTF16("c@gmail.com");
  results.push_back(std::move(saved_form));
  form_manager()->OnGetPasswordStoreResults(std::move(results));
  EXPECT_EQ(2U, form_manager()->best_matches().size());
}

TEST_F(PasswordFormManagerTest, ProcessFrame) {
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_));
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);
}

TEST_F(PasswordFormManagerTest, ProcessFrame_MoreProcessFrameMoreFill) {
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_)).Times(2);
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);
  form_manager()->ProcessFrame(client()->mock_driver()->AsWeakPtr());
}

TEST_F(PasswordFormManagerTest, ProcessFrame_TwoDrivers) {
  NiceMock<MockPasswordManagerDriver> second_driver;

  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_));
  EXPECT_CALL(second_driver, FillPasswordForm(_));
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);
  form_manager()->ProcessFrame(second_driver.AsWeakPtr());
}

TEST_F(PasswordFormManagerTest, ProcessFrame_DriverBeforeMatching) {
  NiceMock<MockPasswordManagerDriver> extra_driver;

  EXPECT_CALL(extra_driver, FillPasswordForm(_));

  // Ask store for logins, but store should not respond yet.
  EXPECT_CALL(*mock_store(),
              GetLogins(form_manager()->observed_form(), _, form_manager()));
  form_manager()->FetchDataFromPasswordStore(PasswordStore::DISALLOW_PROMPT);

  // Now add the extra driver.
  form_manager()->ProcessFrame(extra_driver.AsWeakPtr());

  // Password store responds.
  scoped_ptr<PasswordForm> match(new PasswordForm(*saved_match()));
  ScopedVector<PasswordForm> result_form;
  result_form.push_back(std::move(match));
  form_manager()->OnGetPasswordStoreResults(std::move(result_form));
}

TEST_F(PasswordFormManagerTest, ProcessFrame_StoreUpdatesCausesAutofill) {
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_)).Times(2);
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);
}

TEST_F(PasswordFormManagerTest, UpdateFormManagers_IsCalled) {
  // Let |password_manager()| create one additional PasswordFormManager.
  PasswordStoreConsumer* consumer = nullptr;  // Will point to the new PFM.
  EXPECT_CALL(*mock_store(), GetLogins(_, _, _))
      .WillOnce(SaveArg<2>(&consumer));
  PasswordForm form;
  std::vector<PasswordForm> observed;
  observed.push_back(form);
  password_manager()->OnPasswordFormsParsed(client()->mock_driver(), observed);
  // Make sure that the additional PFM is in POST_MATCHING phase.
  ASSERT_TRUE(consumer);
  consumer->OnGetPasswordStoreResults(ScopedVector<PasswordForm>());

  // Now prepare |form_manager()| for saving.
  SimulateMatchingPhase(form_manager(), RESULT_NO_MATCH);
  PasswordForm saved_form(*observed_form());
  saved_form.preferred = true;
  form_manager()->ProvisionallySave(
      saved_form, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Firing Save() should cause PasswordManager::UpdateFormManagers to make the
  // additional PFM to call the password store again.
  EXPECT_CALL(*mock_store(), GetLogins(_, _, _));
  form_manager()->Save();
}

TEST_F(PasswordFormManagerTest, UploadChangePasswordForm_NEW_PASSWORD) {
  ChangePasswordUploadTest(autofill::NEW_PASSWORD);
}

TEST_F(PasswordFormManagerTest,
       UploadChangePasswordForm_PROBABLY_NEW_PASSWORD) {
  ChangePasswordUploadTest(autofill::PROBABLY_NEW_PASSWORD);
}

TEST_F(PasswordFormManagerTest, UploadChangePasswordForm_NOT_NEW_PASSWORD) {
  ChangePasswordUploadTest(autofill::NOT_NEW_PASSWORD);
}

TEST_F(PasswordFormManagerTest, TestUpdatePSLMatchedCredentials) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  SimulateMatchingPhase(&form_manager, RESULT_SAVED_MATCH | RESULT_PSL_MATCH);

  // User submits a credentials with an old username and a new password.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());

  // Trigger saving to exercise some special case handling in UpdateLogin().
  PasswordForm new_credentials[2];
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(testing::SaveArg<0>(&new_credentials[0]))
      .WillOnce(testing::SaveArg<0>(&new_credentials[1]));

  form_manager.Save();
  Mock::VerifyAndClearExpectations(mock_store());

  // No meta-information should be updated, only the password.
  EXPECT_EQ(credentials.password_value, new_credentials[0].password_value);
  EXPECT_EQ(saved_match()->username_value, new_credentials[0].username_value);
  EXPECT_EQ(saved_match()->username_element,
            new_credentials[0].username_element);
  EXPECT_EQ(saved_match()->password_element,
            new_credentials[0].password_element);
  EXPECT_EQ(saved_match()->origin, new_credentials[0].origin);

  EXPECT_EQ(credentials.password_value, new_credentials[1].password_value);
  EXPECT_EQ(psl_saved_match()->username_element,
            new_credentials[1].username_element);
  EXPECT_EQ(psl_saved_match()->username_element,
            new_credentials[1].username_element);
  EXPECT_EQ(psl_saved_match()->password_element,
            new_credentials[1].password_element);
  EXPECT_EQ(psl_saved_match()->origin, new_credentials[1].origin);
}

TEST_F(PasswordFormManagerTest,
       TestNotUpdatePSLMatchedCredentialsWithAnotherUsername) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  psl_saved_match()->username_value += ASCIIToUTF16("1");
  SimulateMatchingPhase(&form_manager, RESULT_SAVED_MATCH | RESULT_PSL_MATCH);

  // User submits a credentials with an old username and a new password.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());

  // Trigger saving to exercise some special case handling in UpdateLogin().
  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(testing::SaveArg<0>(&new_credentials));

  form_manager.Save();
  Mock::VerifyAndClearExpectations(mock_store());

  // No meta-information should be updated, only the password.
  EXPECT_EQ(credentials.password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_value, new_credentials.username_value);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->origin, new_credentials.origin);
}

TEST_F(PasswordFormManagerTest,
       TestNotUpdatePSLMatchedCredentialsWithAnotherPassword) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  psl_saved_match()->password_value += ASCIIToUTF16("1");
  SimulateMatchingPhase(&form_manager, RESULT_SAVED_MATCH | RESULT_PSL_MATCH);

  // User submits a credentials with an old username and a new password.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());

  // Trigger saving to exercise some special case handling in UpdateLogin().
  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_))
      .WillOnce(testing::SaveArg<0>(&new_credentials));

  form_manager.Save();
  Mock::VerifyAndClearExpectations(mock_store());

  // No meta-information should be updated, only the password.
  EXPECT_EQ(credentials.password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_value, new_credentials.username_value);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->origin, new_credentials.origin);
}

TEST_F(PasswordFormManagerTest, TestNotUpdateWhenOnlyPSLMatched) {
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(), false);
  SimulateMatchingPhase(&form_manager, RESULT_PSL_MATCH);

  // User submits a credentials with an old username and a new password.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager.ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_TRUE(form_manager.IsNewLogin());

  // PSL matched credential should not be updated, since we are not sure that
  // this is the same credential as submitted one.
  PasswordForm new_credentials;
  EXPECT_CALL(*mock_store(), UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store(), AddLogin(_))
      .WillOnce(testing::SaveArg<0>(&new_credentials));

  form_manager.Save();
  Mock::VerifyAndClearExpectations(mock_store());

  EXPECT_EQ(credentials.password_value, new_credentials.password_value);
  EXPECT_EQ(credentials.username_value, new_credentials.username_value);
  EXPECT_EQ(credentials.password_element, new_credentials.password_element);
  EXPECT_EQ(credentials.username_element, new_credentials.username_element);
  EXPECT_EQ(credentials.origin, new_credentials.origin);
}

#if !defined(OS_IOS) && !defined(OS_ANDROID)
TEST_F(PasswordFormManagerTest, FetchStatistics) {
  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  InteractionsStats stats;
  stats.origin_domain = observed_form()->origin.GetOrigin();
  stats.username_value = saved_match()->username_value;
  stats.dismissal_count = 5;
  EXPECT_CALL(*mock_store(),
              GetLogins(*observed_form(), auth_policy, form_manager()));
  std::vector<InteractionsStats*> db_stats;
  db_stats.push_back(new InteractionsStats(stats));
  EXPECT_CALL(*mock_store(), GetSiteStatsMock(stats.origin_domain))
      .WillOnce(Return(db_stats));
  form_manager()->FetchDataFromPasswordStore(auth_policy);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(form_manager()->interactions_stats(),
              ElementsAre(Pointee(stats)));
}
#else
TEST_F(PasswordFormManagerTest, DontFetchStatistics) {
  const PasswordStore::AuthorizationPromptPolicy auth_policy =
      PasswordStore::DISALLOW_PROMPT;
  EXPECT_CALL(*mock_store(),
              GetLogins(*observed_form(), auth_policy, form_manager()));
  EXPECT_CALL(*mock_store(), GetSiteStatsMock(_)).Times(0);
  form_manager()->FetchDataFromPasswordStore(auth_policy);
  base::RunLoop().RunUntilIdle();
}
#endif

TEST_F(PasswordFormManagerTest,
       TestSavingOnChangePasswordFormGenerationNoStoredForms) {
  client()->set_is_update_password_ui_enabled(true);
  SimulateMatchingPhase(form_manager(), RESULT_NO_MATCH);
  form_manager()->set_has_generated_password(true);

  // User submits change password form and there is no stored credentials.
  PasswordForm credentials = *observed_form();
  credentials.username_element.clear();
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_element = ASCIIToUTF16("NewPasswd");
  credentials.new_password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager()->ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, which should know this is a new login.
  EXPECT_TRUE(form_manager()->IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match submitted form.
  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager()->pending_credentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager()->pending_credentials().signon_realm);
  EXPECT_EQ(observed_form()->action,
            form_manager()->pending_credentials().action);
  EXPECT_TRUE(form_manager()->pending_credentials().preferred);
  EXPECT_EQ(ASCIIToUTF16("new_password"),
            form_manager()->pending_credentials().password_value);
  EXPECT_EQ(base::string16(),
            form_manager()->pending_credentials().username_value);
  EXPECT_TRUE(
      form_manager()->pending_credentials().new_password_element.empty());
  EXPECT_TRUE(form_manager()->pending_credentials().new_password_value.empty());
}

TEST_F(PasswordFormManagerTest, TestUpdatingOnChangePasswordFormGeneration) {
  client()->set_is_update_password_ui_enabled(true);
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);
  form_manager()->set_has_generated_password(true);

  // User submits credentials for the change password form, and old password is
  // coincide with password from an existing credentials, so stored credentials
  // should be updated.
  PasswordForm credentials = *observed_form();
  credentials.username_element.clear();
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_element = ASCIIToUTF16("NewPasswd");
  credentials.new_password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager()->ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_FALSE(form_manager()->IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db.
  EXPECT_EQ(saved_match()->origin.spec(),
            form_manager()->pending_credentials().origin.spec());
  EXPECT_EQ(saved_match()->signon_realm,
            form_manager()->pending_credentials().signon_realm);
  EXPECT_EQ(observed_form()->action,
            form_manager()->pending_credentials().action);
  EXPECT_TRUE(form_manager()->pending_credentials().preferred);
  EXPECT_EQ(ASCIIToUTF16("new_password"),
            form_manager()->pending_credentials().password_value);
  EXPECT_EQ(saved_match()->username_value,
            form_manager()->pending_credentials().username_value);
  EXPECT_TRUE(
      form_manager()->pending_credentials().new_password_element.empty());
  EXPECT_TRUE(form_manager()->pending_credentials().new_password_value.empty());
}

TEST_F(PasswordFormManagerTest,
       TestSavingOnChangePasswordFormGenerationNoMatchedForms) {
  client()->set_is_update_password_ui_enabled(true);
  SimulateMatchingPhase(form_manager(), RESULT_SAVED_MATCH);
  form_manager()->set_has_generated_password(true);

  // User submits credentials for the change password form, and old password is
  // not coincide with password from existing credentials, so new credentials
  // should be saved.
  PasswordForm credentials = *observed_form();
  credentials.username_element.clear();
  credentials.password_value =
      saved_match()->password_value + ASCIIToUTF16("1");
  credentials.new_password_element = ASCIIToUTF16("NewPasswd");
  credentials.new_password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager()->ProvisionallySave(
      credentials, PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  EXPECT_TRUE(form_manager()->IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match submitted form.
  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager()->pending_credentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager()->pending_credentials().signon_realm);
  EXPECT_EQ(observed_form()->action,
            form_manager()->pending_credentials().action);
  EXPECT_TRUE(form_manager()->pending_credentials().preferred);
  EXPECT_EQ(ASCIIToUTF16("new_password"),
            form_manager()->pending_credentials().password_value);
  EXPECT_EQ(base::string16(),
            form_manager()->pending_credentials().username_value);
  EXPECT_TRUE(
      form_manager()->pending_credentials().new_password_element.empty());
  EXPECT_TRUE(form_manager()->pending_credentials().new_password_value.empty());
}

}  // namespace password_manager
