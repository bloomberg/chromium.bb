// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/passwords_helper.h"

#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_form_data.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

using autofill::PasswordForm;
using password_manager::PasswordStore;
using sync_datatype_helper::test;

const std::string kFakeSignonRealm = "http://fake-signon-realm.google.com/";
const char* kIndexedFakeOrigin = "http://fake-signon-realm.google.com/%d";

namespace {

// We use a WaitableEvent to wait when logins are added, removed, or updated
// instead of running the UI message loop because of a restriction that
// prevents a DB thread from initiating a quit of the UI message loop.
void PasswordStoreCallback(base::WaitableEvent* wait_event) {
  // Wake up passwords_helper::AddLogin.
  wait_event->Signal();
}

class PasswordStoreConsumerHelper
    : public password_manager::PasswordStoreConsumer {
 public:
  explicit PasswordStoreConsumerHelper(std::vector<PasswordForm>* result)
      : password_manager::PasswordStoreConsumer(), result_(result) {}

  virtual void OnGetPasswordStoreResults(
      const std::vector<PasswordForm*>& result) OVERRIDE {
    result_->clear();
    for (std::vector<PasswordForm*>::const_iterator it = result.begin();
         it != result.end();
         ++it) {
      result_->push_back(**it);
      delete *it;
    }

    // Quit the message loop to wake up passwords_helper::GetLogins.
    base::MessageLoopForUI::current()->Quit();
  }

 private:
  std::vector<PasswordForm>* result_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreConsumerHelper);
};

// PasswordForm::date_synced is a local field. Therefore it may be different
// across clients.
void ClearSyncDateField(std::vector<PasswordForm>* forms) {
  for (std::vector<PasswordForm>::iterator it = forms->begin();
       it != forms->end();
       ++it) {
    it->date_synced = base::Time();
  }
}

}  // namespace

namespace passwords_helper {

void AddLogin(PasswordStore* store, const PasswordForm& form) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(true, false);
  store->AddLogin(form);
  store->ScheduleTask(base::Bind(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

void UpdateLogin(PasswordStore* store, const PasswordForm& form) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(true, false);
  store->UpdateLogin(form);
  store->ScheduleTask(base::Bind(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

void GetLogins(PasswordStore* store, std::vector<PasswordForm>& matches) {
  ASSERT_TRUE(store);
  PasswordForm matcher_form;
  matcher_form.signon_realm = kFakeSignonRealm;
  PasswordStoreConsumerHelper consumer(&matches);
  store->GetLogins(matcher_form, PasswordStore::DISALLOW_PROMPT, &consumer);
  content::RunMessageLoop();
}

void RemoveLogin(PasswordStore* store, const PasswordForm& form) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(true, false);
  store->RemoveLogin(form);
  store->ScheduleTask(base::Bind(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

void RemoveLogins(PasswordStore* store) {
  std::vector<PasswordForm> forms;
  GetLogins(store, forms);
  for (std::vector<PasswordForm>::iterator it = forms.begin();
       it != forms.end(); ++it) {
    RemoveLogin(store, *it);
  }
}

void SetEncryptionPassphrase(int index,
                             const std::string& passphrase,
                             ProfileSyncService::PassphraseType type) {
  ProfileSyncServiceFactory::GetForProfile(
      test()->GetProfile(index))->SetEncryptionPassphrase(passphrase, type);
}

bool SetDecryptionPassphrase(int index, const std::string& passphrase) {
  return ProfileSyncServiceFactory::GetForProfile(
      test()->GetProfile(index))->SetDecryptionPassphrase(passphrase);
}

PasswordStore* GetPasswordStore(int index) {
  return PasswordStoreFactory::GetForProfile(test()->GetProfile(index),
                                             Profile::IMPLICIT_ACCESS).get();
}

PasswordStore* GetVerifierPasswordStore() {
  return PasswordStoreFactory::GetForProfile(test()->verifier(),
                                             Profile::IMPLICIT_ACCESS).get();
}

bool ProfileContainsSamePasswordFormsAsVerifier(int index) {
  std::vector<PasswordForm> verifier_forms;
  std::vector<PasswordForm> forms;
  GetLogins(GetVerifierPasswordStore(), verifier_forms);
  GetLogins(GetPasswordStore(index), forms);
  ClearSyncDateField(&forms);
  bool result =
      password_manager::ContainsSamePasswordForms(verifier_forms, forms);
  if (!result) {
    LOG(ERROR) << "Password forms in Verifier Profile:";
    for (std::vector<PasswordForm>::iterator it = verifier_forms.begin();
         it != verifier_forms.end(); ++it) {
      LOG(ERROR) << *it << std::endl;
    }
    LOG(ERROR) << "Password forms in Profile" << index << ":";
    for (std::vector<PasswordForm>::iterator it = forms.begin();
         it != forms.end(); ++it) {
      LOG(ERROR) << *it << std::endl;
    }
  }
  return result;
}

bool ProfilesContainSamePasswordForms(int index_a, int index_b) {
  std::vector<PasswordForm> forms_a;
  std::vector<PasswordForm> forms_b;
  GetLogins(GetPasswordStore(index_a), forms_a);
  GetLogins(GetPasswordStore(index_b), forms_b);
  ClearSyncDateField(&forms_a);
  ClearSyncDateField(&forms_b);
  bool result = password_manager::ContainsSamePasswordForms(forms_a, forms_b);
  if (!result) {
    LOG(ERROR) << "Password forms in Profile" << index_a << ":";
    for (std::vector<PasswordForm>::iterator it = forms_a.begin();
         it != forms_a.end(); ++it) {
      LOG(ERROR) << *it << std::endl;
    }
    LOG(ERROR) << "Password forms in Profile" << index_b << ":";
    for (std::vector<PasswordForm>::iterator it = forms_b.begin();
         it != forms_b.end(); ++it) {
      LOG(ERROR) << *it << std::endl;
    }
  }
  return result;
}

bool AllProfilesContainSamePasswordFormsAsVerifier() {
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (!ProfileContainsSamePasswordFormsAsVerifier(i)) {
      DVLOG(1) << "Profile " << i << " does not contain the same password"
                                     " forms as the verifier.";
      return false;
    }
  }
  return true;
}

bool AllProfilesContainSamePasswordForms() {
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!ProfilesContainSamePasswordForms(0, i)) {
      DVLOG(1) << "Profile " << i << " does not contain the same password"
                                     " forms as Profile 0.";
      return false;
    }
  }
  return true;
}

namespace {

// Helper class used in the implementation of
// AwaitAllProfilesContainSamePasswordForms.
class SamePasswordFormsChecker : public MultiClientStatusChangeChecker {
 public:
  SamePasswordFormsChecker();
  virtual ~SamePasswordFormsChecker();

  virtual bool IsExitConditionSatisfied() OVERRIDE;
  virtual std::string GetDebugMessage() const OVERRIDE;

 private:
  bool in_progress_;
  bool needs_recheck_;
};

SamePasswordFormsChecker::SamePasswordFormsChecker()
    : MultiClientStatusChangeChecker(
        sync_datatype_helper::test()->GetSyncServices()),
      in_progress_(false),
      needs_recheck_(false) {}

SamePasswordFormsChecker::~SamePasswordFormsChecker() {}

// This method needs protection against re-entrancy.
//
// This function indirectly calls GetLogins(), which starts a RunLoop on the UI
// thread.  This can be a problem, since the next task to execute could very
// well contain a ProfileSyncService::OnStateChanged() event, which would
// trigger another call to this here function, and start another layer of
// nested RunLoops.  That makes the StatusChangeChecker's Quit() method
// ineffective.
//
// The work-around is to not allow re-entrancy.  But we can't just drop
// IsExitConditionSatisifed() calls if one is already in progress.  Instead, we
// set a flag to ask the current execution of IsExitConditionSatisfied() to be
// re-run.  This ensures that the return value is always based on the most
// up-to-date state.
bool SamePasswordFormsChecker::IsExitConditionSatisfied() {
  if (in_progress_) {
    LOG(WARNING) << "Setting flag and returning early to prevent nesting.";
    needs_recheck_ = true;
    return false;
  }

  // Keep retrying until we get a good reading.
  bool result = false;
  in_progress_ = true;
  do {
    needs_recheck_ = false;
    result = AllProfilesContainSamePasswordForms();
  } while (needs_recheck_);
  in_progress_ = false;
  return result;
}

std::string SamePasswordFormsChecker::GetDebugMessage() const {
  return "Waiting for matching passwords";
}

}  //  namespace

bool AwaitAllProfilesContainSamePasswordForms() {
  SamePasswordFormsChecker checker;
  checker.Wait();
  return !checker.TimedOut();
}

namespace {

// Helper class used in the implementation of
// AwaitProfileContainSamePasswordFormsAsVerifier.
class SamePasswordFormsAsVerifierChecker
    : public SingleClientStatusChangeChecker {
 public:
  explicit SamePasswordFormsAsVerifierChecker(int index);
  virtual ~SamePasswordFormsAsVerifierChecker();

  virtual bool IsExitConditionSatisfied() OVERRIDE;
  virtual std::string GetDebugMessage() const OVERRIDE;

 private:
  int index_;

  bool in_progress_;
  bool needs_recheck_;
};

SamePasswordFormsAsVerifierChecker::SamePasswordFormsAsVerifierChecker(int i)
    : SingleClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncService(i)),
      index_(i),
      in_progress_(false),
      needs_recheck_(false) {
}

SamePasswordFormsAsVerifierChecker::~SamePasswordFormsAsVerifierChecker() {
}

// This method uses the same re-entrancy prevention trick as
// the SamePasswordFormsChecker.
bool SamePasswordFormsAsVerifierChecker::IsExitConditionSatisfied() {
  if (in_progress_) {
    LOG(WARNING) << "Setting flag and returning early to prevent nesting.";
    needs_recheck_ = true;
    return false;
  }

  // Keep retrying until we get a good reading.
  bool result = false;
  in_progress_ = true;
  do {
    needs_recheck_ = false;
    result = ProfileContainsSamePasswordFormsAsVerifier(index_);
  } while (needs_recheck_);
  in_progress_ = false;
  return result;
}

std::string SamePasswordFormsAsVerifierChecker::GetDebugMessage() const {
  return "Waiting for passwords to match verifier";
}

}  //  namespace

bool AwaitProfileContainsSamePasswordFormsAsVerifier(int index) {
  SamePasswordFormsAsVerifierChecker checker(index);
  checker.Wait();
  return !checker.TimedOut();
}

int GetPasswordCount(int index) {
  std::vector<PasswordForm> forms;
  GetLogins(GetPasswordStore(index), forms);
  return forms.size();
}

int GetVerifierPasswordCount() {
  std::vector<PasswordForm> verifier_forms;
  GetLogins(GetVerifierPasswordStore(), verifier_forms);
  return verifier_forms.size();
}

PasswordForm CreateTestPasswordForm(int index) {
  PasswordForm form;
  form.signon_realm = kFakeSignonRealm;
  form.origin = GURL(base::StringPrintf(kIndexedFakeOrigin, index));
  form.username_value =
      base::ASCIIToUTF16(base::StringPrintf("username%d", index));
  form.password_value =
      base::ASCIIToUTF16(base::StringPrintf("password%d", index));
  form.date_created = base::Time::Now();
  return form;
}

}  // namespace passwords_helper
