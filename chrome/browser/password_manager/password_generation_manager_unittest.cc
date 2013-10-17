// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_generation_manager.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Unlike the base AutofillMetrics, exposes copy and assignment constructors,
// which are handy for briefer test code.  The AutofillMetrics class is
// stateless, so this is safe.
class TestAutofillMetrics : public autofill::AutofillMetrics {
 public:
  TestAutofillMetrics() {}
  virtual ~TestAutofillMetrics() {}
};

}  // anonymous namespace

class TestPasswordGenerationManager : public PasswordGenerationManager {
 public:
  explicit TestPasswordGenerationManager(content::WebContents* contents)
      : PasswordGenerationManager(contents) {}
  virtual ~TestPasswordGenerationManager() {}

  virtual void SendAccountCreationFormsToRenderer(
      content::RenderViewHost* host,
      const std::vector<autofill::FormData>& forms) OVERRIDE {
    sent_account_creation_forms_.insert(
        sent_account_creation_forms_.begin(), forms.begin(), forms.end());
  }

  const std::vector<autofill::FormData>& GetSentAccountCreationForms() {
    return sent_account_creation_forms_;
  }

  void ClearSentAccountCreationForms() {
    sent_account_creation_forms_.clear();
  }

 private:
  std::vector<autofill::FormData> sent_account_creation_forms_;

  DISALLOW_COPY_AND_ASSIGN(TestPasswordGenerationManager);
};

class PasswordGenerationManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual void SetUp() OVERRIDE {
    SetThreadBundleOptions(content::TestBrowserThreadBundle::REAL_IO_THREAD);
    ChromeRenderViewHostTestHarness::SetUp();

    password_generation_manager_.reset(
        new TestPasswordGenerationManager(web_contents()));
  }

  virtual void TearDown() OVERRIDE {
    ChromeRenderViewHostTestHarness::TearDown();
  }

  bool IsGenerationEnabled() {
    return password_generation_manager_->IsGenerationEnabled();
  }

  void DetectAccountCreationForms(
      const std::vector<autofill::FormStructure*>& forms) {
    password_generation_manager_->DetectAccountCreationForms(forms);
  }

  scoped_ptr<TestPasswordGenerationManager> password_generation_manager_;
};

class IncognitoPasswordGenerationManagerTest :
    public PasswordGenerationManagerTest {
 public:
  virtual content::BrowserContext* CreateBrowserContext() OVERRIDE {
    // Create an incognito profile.
    TestingProfile::Builder builder;
    builder.SetIncognito();
    scoped_ptr<TestingProfile> profile = builder.Build();
    return profile.release();
  }
};

TEST_F(PasswordGenerationManagerTest, IsGenerationEnabled) {
  PasswordManagerDelegateImpl::CreateForWebContents(web_contents());
  PasswordManager::CreateForWebContentsAndDelegate(
      web_contents(),
      PasswordManagerDelegateImpl::FromWebContents(web_contents()));

  PrefService* prefs = profile()->GetPrefs();

  // Always set password sync enabled so we can test the behavior of password
  // generation.
  prefs->SetBoolean(prefs::kSyncKeepEverythingSynced, false);
  ProfileSyncService* sync_service = ProfileSyncServiceFactory::GetForProfile(
      profile());
  sync_service->SetSyncSetupCompleted();
  syncer::ModelTypeSet preferred_set;
  preferred_set.Put(syncer::PASSWORDS);
  sync_service->ChangePreferredDataTypes(preferred_set);

  // Pref is false, should not be enabled.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, false);
  EXPECT_FALSE(IsGenerationEnabled());

  // Pref is true, should be enabled.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, true);
  EXPECT_TRUE(IsGenerationEnabled());

  // Change syncing preferences to not include passwords. Generation should
  // be disabled.
  preferred_set.Put(syncer::EXTENSIONS);
  preferred_set.Remove(syncer::PASSWORDS);
  sync_service->ChangePreferredDataTypes(preferred_set);
  EXPECT_FALSE(IsGenerationEnabled());

  // Disable syncing. Generation should also be disabled.
  sync_service->DisableForUser();
  EXPECT_FALSE(IsGenerationEnabled());
}

TEST_F(PasswordGenerationManagerTest, DetectAccountCreationForms) {
  // Setup so that IsGenerationEnabled() returns true.
  PasswordManagerDelegateImpl::CreateForWebContents(web_contents());
  PasswordManager::CreateForWebContentsAndDelegate(
      web_contents(),
      PasswordManagerDelegateImpl::FromWebContents(web_contents()));

  ProfileSyncService* sync_service = ProfileSyncServiceFactory::GetForProfile(
      profile());
  sync_service->SetSyncSetupCompleted();

  profile()->GetPrefs()->SetBoolean(prefs::kPasswordGenerationEnabled, true);

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
      kServerResponse,
      forms,
      TestAutofillMetrics());

  DetectAccountCreationForms(forms);
  EXPECT_EQ(1u,
            password_generation_manager_->GetSentAccountCreationForms().size());
  EXPECT_EQ(
      GURL("http://accounts.yahoo.com/"),
      password_generation_manager_->GetSentAccountCreationForms()[0].origin);
}

TEST_F(IncognitoPasswordGenerationManagerTest,
       UpdatePasswordSyncStateIncognito) {
  // Disable password manager by going incognito. Even though syncing is
  // enabled, generation should still be disabled.
  PasswordManagerDelegateImpl::CreateForWebContents(web_contents());
  PasswordManager::CreateForWebContentsAndDelegate(
      web_contents(),
      PasswordManagerDelegateImpl::FromWebContents(web_contents()));

  PrefService* prefs = profile()->GetPrefs();

  // Allow this test to control what should get synced.
  prefs->SetBoolean(prefs::kSyncKeepEverythingSynced, false);
  // Always set password generation enabled check box so we can test the
  // behavior of password sync.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, true);

  browser_sync::SyncPrefs sync_prefs(profile()->GetPrefs());
  sync_prefs.SetSyncSetupCompleted();
  EXPECT_FALSE(IsGenerationEnabled());
}
