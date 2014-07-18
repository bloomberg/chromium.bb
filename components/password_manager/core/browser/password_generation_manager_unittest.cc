// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

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
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace password_manager {

namespace {

class TestPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  TestPasswordManagerDriver(PasswordManagerClient* client)
      : password_manager_(client),
        password_generation_manager_(client),
        password_autofill_manager_(client, NULL),
        is_off_the_record_(false) {}
  virtual ~TestPasswordManagerDriver() {}

  // PasswordManagerDriver implementation.
  virtual bool IsOffTheRecord() OVERRIDE { return is_off_the_record_; }
  virtual PasswordGenerationManager* GetPasswordGenerationManager() OVERRIDE {
    return &password_generation_manager_;
  }
  virtual PasswordManager* GetPasswordManager() OVERRIDE {
    return &password_manager_;
  }
  virtual PasswordAutofillManager* GetPasswordAutofillManager() OVERRIDE {
    return &password_autofill_manager_;
  }
  virtual void AccountCreationFormsFound(
      const std::vector<autofill::FormData>& forms) OVERRIDE {
    found_account_creation_forms_.insert(
        found_account_creation_forms_.begin(), forms.begin(), forms.end());
  }

  const std::vector<autofill::FormData>& GetFoundAccountCreationForms() {
    return found_account_creation_forms_;
  }
  void set_is_off_the_record(bool is_off_the_record) {
    is_off_the_record_ = is_off_the_record;
  }

 private:
  PasswordManager password_manager_;
  PasswordGenerationManager password_generation_manager_;
  PasswordAutofillManager password_autofill_manager_;
  std::vector<autofill::FormData> found_account_creation_forms_;
  bool is_off_the_record_;
};

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  TestPasswordManagerClient(scoped_ptr<PrefService> prefs)
      : prefs_(prefs.Pass()), driver_(this), is_sync_enabled_(false) {}

  virtual PasswordStore* GetPasswordStore() OVERRIDE { return NULL; }
  virtual PrefService* GetPrefs() OVERRIDE { return prefs_.get(); }
  virtual PasswordManagerDriver* GetDriver() OVERRIDE { return &driver_; }
  virtual void AuthenticateAutofillAndFillForm(
      scoped_ptr<autofill::PasswordFormFillData> fill_data) OVERRIDE {}
  virtual bool IsPasswordSyncEnabled() OVERRIDE { return is_sync_enabled_; }

  void set_is_password_sync_enabled(bool enabled) {
    is_sync_enabled_ = enabled;
  }

 private:
  scoped_ptr<PrefService> prefs_;
  TestPasswordManagerDriver driver_;
  bool is_sync_enabled_;
};

// Unlike the base AutofillMetrics, exposes copy and assignment constructors,
// which are handy for briefer test code.  The AutofillMetrics class is
// stateless, so this is safe.
class TestAutofillMetrics : public autofill::AutofillMetrics {
 public:
  TestAutofillMetrics() {}
  virtual ~TestAutofillMetrics() {}
};

}  // anonymous namespace

class PasswordGenerationManagerTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    // Construct a PrefService and register all necessary prefs before handing
    // it off to |client_|, as the initialization flow of |client_| will
    // indirectly cause those prefs to be immediately accessed.
    scoped_ptr<TestingPrefServiceSimple> prefs(new TestingPrefServiceSimple());
    prefs->registry()->RegisterBooleanPref(prefs::kPasswordManagerSavingEnabled,
                                           true);
    client_.reset(new TestPasswordManagerClient(prefs.PassAs<PrefService>()));
  }

  virtual void TearDown() OVERRIDE { client_.reset(); }

  PasswordGenerationManager* GetGenerationManager() {
    return client_->GetDriver()->GetPasswordGenerationManager();
  }

  TestPasswordManagerDriver* GetTestDriver() {
    return static_cast<TestPasswordManagerDriver*>(client_->GetDriver());
  }

  bool IsGenerationEnabled() {
    return GetGenerationManager()->IsGenerationEnabled();
  }

  void DetectAccountCreationForms(
      const std::vector<autofill::FormStructure*>& forms) {
    GetGenerationManager()->DetectAccountCreationForms(forms);
  }

  scoped_ptr<TestPasswordManagerClient> client_;
};

TEST_F(PasswordGenerationManagerTest, IsGenerationEnabled) {
  // Enabling the PasswordManager and password sync should cause generation to
  // be enabled.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetBoolean(prefs::kPasswordManagerSavingEnabled, true);
  client_->set_is_password_sync_enabled(true);
  EXPECT_TRUE(IsGenerationEnabled());

  // Disabling password syncing should cause generation to be disabled.
  client_->set_is_password_sync_enabled(false);
  EXPECT_FALSE(IsGenerationEnabled());

  // Disabling the PasswordManager should cause generation to be disabled even
  // if syncing is enabled.
  prefs->SetBoolean(prefs::kPasswordManagerSavingEnabled, false);
  client_->set_is_password_sync_enabled(true);
  EXPECT_FALSE(IsGenerationEnabled());
}

TEST_F(PasswordGenerationManagerTest, DetectAccountCreationForms) {
  // Setup so that IsGenerationEnabled() returns true.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetBoolean(prefs::kPasswordManagerSavingEnabled, true);
  client_->set_is_password_sync_enabled(true);

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
  autofill::FormStructure::ParseQueryResponse(
      kServerResponse, forms, TestAutofillMetrics());

  DetectAccountCreationForms(forms);
  EXPECT_EQ(1u, GetTestDriver()->GetFoundAccountCreationForms().size());
  EXPECT_EQ(GURL("http://accounts.yahoo.com/"),
            GetTestDriver()->GetFoundAccountCreationForms()[0].origin);
}

TEST_F(PasswordGenerationManagerTest, UpdatePasswordSyncStateIncognito) {
  // Disable password manager by going incognito. Even though password
  // syncing is enabled, generation should still
  // be disabled.
  GetTestDriver()->set_is_off_the_record(true);
  PrefService* prefs = client_->GetPrefs();
  prefs->SetBoolean(prefs::kPasswordManagerSavingEnabled, true);
  client_->set_is_password_sync_enabled(true);

  EXPECT_FALSE(IsGenerationEnabled());
}

}  // namespace password_manager
