// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_passwords_sync_test.h"

#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/password_manager/password_store_consumer.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/browser_thread.h"

using webkit_glue::PasswordForm;

const std::string kFakeSignonRealm = "http://fake-signon-realm.google.com/";

// We use a WaitableEvent to wait on AddLogin instead of running the UI message
// loop because of a restriction that prevents a DB thread from initiating a
// quit of the UI message loop.
void PasswordStoreCallback(base::WaitableEvent* wait_event) {
  // Wake up LivePasswordsSyncTest::AddLogin.
  wait_event->Signal();
}

class PasswordStoreConsumerHelper : public PasswordStoreConsumer {
 public:
  explicit PasswordStoreConsumerHelper(std::vector<PasswordForm>* result)
      : PasswordStoreConsumer(),
        result_(result) {}

  virtual void OnPasswordStoreRequestDone(
      CancelableRequestProvider::Handle handle,
      const std::vector<PasswordForm*>& result) {
    result_->clear();
    for (std::vector<PasswordForm*>::const_iterator it = result.begin();
         it != result.end(); ++it) {
      // Make a copy of the form since it gets deallocated after the caller of
      // this method returns.
      result_->push_back(**it);
    }

    // Quit the message loop to wake up LivePasswordsSyncTest::GetLogins.
    MessageLoopForUI::current()->Quit();
  }

 private:
  std::vector<PasswordForm>* result_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreConsumerHelper);
};

LivePasswordsSyncTest::LivePasswordsSyncTest(TestType test_type)
    : LiveSyncTest(test_type) {}

void LivePasswordsSyncTest::CleanUpOnMainThread() {
  CleanupTestPasswordForms();
  LiveSyncTest::CleanUpOnMainThread();
}

bool LivePasswordsSyncTest::SetupClients() {
  if (LiveSyncTest::SetupClients()) {
    CleanupTestPasswordForms();
    return true;
  }
  return false;
}

void LivePasswordsSyncTest::AddLogin(PasswordStore* store,
                                     const PasswordForm& form) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(true, false);
  store->AddLogin(form);
  store->ScheduleTask(NewRunnableFunction(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

void LivePasswordsSyncTest::GetLogins(PasswordStore* store,
                                      std::vector<PasswordForm>& matches) {
  ASSERT_TRUE(store);
  PasswordForm matcher_form;
  matcher_form.signon_realm = kFakeSignonRealm;
  PasswordStoreConsumerHelper consumer(&matches);
  store->GetLogins(matcher_form, &consumer);
  ui_test_utils::RunMessageLoop();
}

void LivePasswordsSyncTest::SetPassphrase(int index,
                                          const std::string& passphrase,
                                          bool is_creation) {
  GetProfile(index)->GetProfileSyncService("")->SetPassphrase(
      passphrase, true, is_creation);
}

PasswordStore* LivePasswordsSyncTest::GetPasswordStore(int index) {
  return GetProfile(index)->GetPasswordStore(Profile::IMPLICIT_ACCESS);
}

PasswordStore* LivePasswordsSyncTest::GetVerifierPasswordStore() {
  return verifier()->GetPasswordStore(Profile::IMPLICIT_ACCESS);
}

bool LivePasswordsSyncTest::ProfileContainsSamePasswordFormsAsVerifier(
    int index) {
  std::vector<PasswordForm> verifier_forms;
  std::vector<PasswordForm> forms;
  GetLogins(GetVerifierPasswordStore(), verifier_forms);
  GetLogins(GetPasswordStore(index), forms);
  return ContainsSamePasswordForms(verifier_forms, forms);
}

bool LivePasswordsSyncTest::ProfilesContainSamePasswordForms(int index_a,
                                                             int index_b) {
  std::vector<PasswordForm> forms_a;
  std::vector<PasswordForm> forms_b;
  GetLogins(GetPasswordStore(index_a), forms_a);
  GetLogins(GetPasswordStore(index_b), forms_b);
  return ContainsSamePasswordForms(forms_a, forms_b);
}

bool LivePasswordsSyncTest::AllProfilesContainSamePasswordFormsAsVerifier() {
  for (int i = 0; i < num_clients(); ++i) {
    if (!ProfileContainsSamePasswordFormsAsVerifier(i)) {
      LOG(ERROR) << "Profile " << i << " does not contain the same password "
                                       " forms as the verifier.";
      return false;
    }
  }
  return true;
}

bool LivePasswordsSyncTest::AllProfilesContainSamePasswordForms() {
  for (int i = 1; i < num_clients(); ++i) {
    if (!ProfilesContainSamePasswordForms(0, i)) {
      LOG(ERROR) << "Profile " << i << " does not contain the same password "
                                       " forms as Profile 0.";
      return false;
    }
  }
  return true;
}

int LivePasswordsSyncTest::GetPasswordCount(int index) {
  std::vector<PasswordForm> forms;
  GetLogins(GetPasswordStore(index), forms);
  return forms.size();
}

int LivePasswordsSyncTest::GetVerifierPasswordCount() {
  std::vector<PasswordForm> verifier_forms;
  GetLogins(GetVerifierPasswordStore(), verifier_forms);
  return verifier_forms.size();
}

PasswordForm LivePasswordsSyncTest::CreateTestPasswordForm(int index) {
  PasswordForm form;
  form.signon_realm = kFakeSignonRealm;
  form.origin =
      GURL(base::StringPrintf("http://fake-domain%d.google.com/", index));
  form.username_value = ASCIIToUTF16(base::StringPrintf("username%d", index));
  form.password_value = ASCIIToUTF16(base::StringPrintf("password%d", index));
  return form;
}

void LivePasswordsSyncTest::CleanupTestPasswordForms() {
  std::vector<PasswordForm> forms;
  GetLogins(GetVerifierPasswordStore(), forms);
  for (std::vector<PasswordForm>::iterator it = forms.begin();
       it != forms.end(); ++it) {
    GetVerifierPasswordStore()->RemoveLogin(*it);
  }
  forms.clear();
  GetLogins(GetVerifierPasswordStore(), forms);
  ASSERT_EQ(0U, forms.size());
}
