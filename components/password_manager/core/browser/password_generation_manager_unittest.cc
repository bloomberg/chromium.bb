// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using testing::_;

namespace password_manager {

namespace {

class TestPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  TestPasswordManagerDriver(PasswordManagerClient* client)
      : password_manager_(client),
        password_generation_manager_(client, this),
        password_autofill_manager_(this, nullptr) {}
  ~TestPasswordManagerDriver() override {}

  // PasswordManagerDriver implementation.
  PasswordGenerationManager* GetPasswordGenerationManager() override {
    return &password_generation_manager_;
  }
  PasswordManager* GetPasswordManager() override { return &password_manager_; }
  PasswordAutofillManager* GetPasswordAutofillManager() override {
    return &password_autofill_manager_;
  }
  void AccountCreationFormsFound(
      const std::vector<autofill::FormData>& forms) override {
    found_account_creation_forms_.insert(
        found_account_creation_forms_.begin(), forms.begin(), forms.end());
  }

  const std::vector<autofill::FormData>& GetFoundAccountCreationForms() {
    return found_account_creation_forms_;
  }

 private:
  PasswordManager password_manager_;
  PasswordGenerationManager password_generation_manager_;
  PasswordAutofillManager password_autofill_manager_;
  std::vector<autofill::FormData> found_account_creation_forms_;
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(GetPasswordSyncState, PasswordSyncState());
  MOCK_CONST_METHOD0(IsSavingAndFillingEnabledForCurrentPage, bool());
  MOCK_CONST_METHOD0(IsOffTheRecord, bool());

  MockPasswordManagerClient(scoped_ptr<PrefService> prefs)
      : prefs_(prefs.Pass()), store_(new TestPasswordStore), driver_(this) {}

  ~MockPasswordManagerClient() override { store_->Shutdown(); }

  PasswordStore* GetPasswordStore() const override { return store_.get(); }
  PrefService* GetPrefs() override { return prefs_.get(); }

  TestPasswordManagerDriver* test_driver() { return &driver_; }

 private:
  scoped_ptr<PrefService> prefs_;
  scoped_refptr<TestPasswordStore> store_;
  TestPasswordManagerDriver driver_;
};

}  // anonymous namespace

class PasswordGenerationManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    // Construct a PrefService and register all necessary prefs before handing
    // it off to |client_|, as the initialization flow of |client_| will
    // indirectly cause those prefs to be immediately accessed.
    scoped_ptr<TestingPrefServiceSimple> prefs(new TestingPrefServiceSimple());
    prefs->registry()->RegisterBooleanPref(prefs::kPasswordManagerSavingEnabled,
                                           true);
    client_.reset(new MockPasswordManagerClient(prefs.Pass()));
  }

  void TearDown() override { client_.reset(); }

  PasswordGenerationManager* GetGenerationManager() {
    return client_->test_driver()->GetPasswordGenerationManager();
  }

  TestPasswordManagerDriver* GetTestDriver() { return client_->test_driver(); }

  bool IsGenerationEnabled() {
    return GetGenerationManager()->IsGenerationEnabled();
  }

  void DetectAccountCreationForms(
      const std::vector<autofill::FormStructure*>& forms) {
    GetGenerationManager()->DetectAccountCreationForms(forms);
  }

  base::MessageLoop message_loop_;
  scoped_ptr<MockPasswordManagerClient> client_;
};

TEST_F(PasswordGenerationManagerTest, IsGenerationEnabled) {
  // Enabling the PasswordManager and password sync should cause generation to
  // be enabled, unless the sync is with a custom passphrase.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_NORMAL_ENCRYPTION));
  EXPECT_TRUE(IsGenerationEnabled());

  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_WITH_CUSTOM_PASSPHRASE));
  EXPECT_FALSE(IsGenerationEnabled());

  // Disabling password syncing should cause generation to be disabled.
  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(NOT_SYNCING_PASSWORDS));
  EXPECT_FALSE(IsGenerationEnabled());

  // Disabling the PasswordManager should cause generation to be disabled even
  // if syncing is enabled.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_NORMAL_ENCRYPTION));
  EXPECT_FALSE(IsGenerationEnabled());

  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_WITH_CUSTOM_PASSPHRASE));
  EXPECT_FALSE(IsGenerationEnabled());
}

TEST_F(PasswordGenerationManagerTest, DetectAccountCreationForms) {
  // Setup so that IsGenerationEnabled() returns true.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_NORMAL_ENCRYPTION));

  autofill::FormData login_form;
  login_form.origin = GURL("http://www.yahoo.com/login/");
  autofill::FormFieldData username;
  username.label = ASCIIToUTF16("username");
  username.name = ASCIIToUTF16("login");
  username.form_control_type = "text";
  login_form.fields.push_back(username);
  autofill::FormFieldData password;
  password.label = ASCIIToUTF16("password");
  password.name = ASCIIToUTF16("password");
  password.form_control_type = "password";
  login_form.fields.push_back(password);
  autofill::FormStructure form1(login_form);
  std::vector<autofill::FormStructure*> forms;
  forms.push_back(&form1);
  autofill::FormData account_creation_form;
  account_creation_form.origin = GURL("http://accounts.yahoo.com/");
  account_creation_form.fields.push_back(username);
  account_creation_form.fields.push_back(password);
  autofill::FormFieldData confirm_password;
  confirm_password.label = ASCIIToUTF16("confirm_password");
  confirm_password.name = ASCIIToUTF16("password");
  confirm_password.form_control_type = "password";
  account_creation_form.fields.push_back(confirm_password);
  autofill::FormStructure form2(account_creation_form);
  forms.push_back(&form2);

  // Simulate the server response to set the field types.
  const char* const kServerResponse =
      "<autofillqueryresponse>"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"75\" />"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"76\" />"
      "<field autofilltype=\"75\" />"
      "</autofillqueryresponse>";
  autofill::FormStructure::ParseQueryResponse(kServerResponse, forms, NULL);

  DetectAccountCreationForms(forms);
  EXPECT_EQ(1u, GetTestDriver()->GetFoundAccountCreationForms().size());
  EXPECT_EQ(GURL("http://accounts.yahoo.com/"),
            GetTestDriver()->GetFoundAccountCreationForms()[0].origin);
}

TEST_F(PasswordGenerationManagerTest, UpdatePasswordSyncStateIncognito) {
  // Disable password manager by going incognito. Even though password
  // syncing is enabled, generation should still
  // be disabled.
  EXPECT_CALL(*client_, IsOffTheRecord()).WillRepeatedly(testing::Return(true));
  PrefService* prefs = client_->GetPrefs();
  prefs->SetBoolean(prefs::kPasswordManagerSavingEnabled, true);
  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_NORMAL_ENCRYPTION));

  EXPECT_FALSE(IsGenerationEnabled());
}

}  // namespace password_manager
