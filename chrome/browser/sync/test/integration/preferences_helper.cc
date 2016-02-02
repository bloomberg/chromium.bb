// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/preferences_helper.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

using sync_datatype_helper::test;

namespace preferences_helper {

PrefService* GetPrefs(int index) {
  return test()->GetProfile(index)->GetPrefs();
}

PrefService* GetVerifierPrefs() {
  return test()->verifier()->GetPrefs();
}

void ChangeBooleanPref(int index, const char* pref_name) {
  bool new_value = !GetPrefs(index)->GetBoolean(pref_name);
  GetPrefs(index)->SetBoolean(pref_name, new_value);
  if (test()->use_verifier())
    GetVerifierPrefs()->SetBoolean(pref_name, new_value);
}

void ChangeIntegerPref(int index, const char* pref_name, int new_value) {
  GetPrefs(index)->SetInteger(pref_name, new_value);
  if (test()->use_verifier())
    GetVerifierPrefs()->SetInteger(pref_name, new_value);
}

void ChangeInt64Pref(int index, const char* pref_name, int64_t new_value) {
  GetPrefs(index)->SetInt64(pref_name, new_value);
  if (test()->use_verifier())
    GetVerifierPrefs()->SetInt64(pref_name, new_value);
}

void ChangeDoublePref(int index, const char* pref_name, double new_value) {
  GetPrefs(index)->SetDouble(pref_name, new_value);
  if (test()->use_verifier())
    GetVerifierPrefs()->SetDouble(pref_name, new_value);
}

void ChangeStringPref(int index,
                      const char* pref_name,
                      const std::string& new_value) {
  GetPrefs(index)->SetString(pref_name, new_value);
  if (test()->use_verifier())
    GetVerifierPrefs()->SetString(pref_name, new_value);
}

void ChangeFilePathPref(int index,
                        const char* pref_name,
                        const base::FilePath& new_value) {
  GetPrefs(index)->SetFilePath(pref_name, new_value);
  if (test()->use_verifier())
    GetVerifierPrefs()->SetFilePath(pref_name, new_value);
}

void ChangeListPref(int index,
                    const char* pref_name,
                    const base::ListValue& new_value) {
  {
    ListPrefUpdate update(GetPrefs(index), pref_name);
    base::ListValue* list = update.Get();
    for (base::ListValue::const_iterator it = new_value.begin();
         it != new_value.end();
         ++it) {
      list->Append((*it)->DeepCopy());
    }
  }

  if (test()->use_verifier()) {
    ListPrefUpdate update_verifier(GetVerifierPrefs(), pref_name);
    base::ListValue* list_verifier = update_verifier.Get();
    for (base::ListValue::const_iterator it = new_value.begin();
         it != new_value.end();
         ++it) {
      list_verifier->Append((*it)->DeepCopy());
    }
  }
}

bool BooleanPrefMatches(const char* pref_name) {
  bool reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetBoolean(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetBoolean(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetBoolean(pref_name)) {
      DVLOG(1) << "Boolean preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool IntegerPrefMatches(const char* pref_name) {
  int reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetInteger(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetInteger(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetInteger(pref_name)) {
      DVLOG(1) << "Integer preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool Int64PrefMatches(const char* pref_name) {
  int64_t reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetInt64(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetInt64(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetInt64(pref_name)) {
      DVLOG(1) << "Integer preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool DoublePrefMatches(const char* pref_name) {
  double reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetDouble(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetDouble(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetDouble(pref_name)) {
      DVLOG(1) << "Double preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool StringPrefMatches(const char* pref_name) {
  std::string reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetString(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetString(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetString(pref_name)) {
      DVLOG(1) << "String preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool FilePathPrefMatches(const char* pref_name) {
  base::FilePath reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetFilePath(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetFilePath(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetFilePath(pref_name)) {
      DVLOG(1) << "base::FilePath preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool ListPrefMatches(const char* pref_name) {
  const base::ListValue* reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetList(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetList(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (!reference_value->Equals(GetPrefs(i)->GetList(pref_name))) {
      DVLOG(1) << "List preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}


namespace {

class PrefMatchChecker : public StatusChangeChecker {
 public:
  explicit PrefMatchChecker(const char* path);
  ~PrefMatchChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied() override = 0;
  std::string GetDebugMessage() const override;

  // Wait for condition to become true.
  void Wait();

 protected:
  const char* GetPath() const;

 private:
  void RegisterPrefListener(PrefService* pref_service);

  ScopedVector<PrefChangeRegistrar> pref_change_registrars_;
  const char* path_;
};

PrefMatchChecker::PrefMatchChecker(const char* path) : path_(path) {
}

PrefMatchChecker::~PrefMatchChecker() {
}

std::string PrefMatchChecker::GetDebugMessage() const {
  return base::StringPrintf("Waiting for pref '%s' to match", GetPath());
}

void PrefMatchChecker::Wait() {
  if (test()->use_verifier()) {
    RegisterPrefListener(GetVerifierPrefs());
  }

  for (int i = 0; i < test()->num_clients(); ++i) {
    RegisterPrefListener(GetPrefs(i));
  }

  if (IsExitConditionSatisfied()) {
    return;
  }

  StartBlockingWait();
}

const char* PrefMatchChecker::GetPath() const {
  return path_;
}

void PrefMatchChecker::RegisterPrefListener(PrefService* pref_service) {
  scoped_ptr<PrefChangeRegistrar> registrar(new PrefChangeRegistrar());
  registrar->Init(pref_service);
  registrar->Add(path_,
                 base::Bind(&PrefMatchChecker::CheckExitCondition,
                            base::Unretained(this)));
  pref_change_registrars_.push_back(registrar.release());
}

// Helper class used in the implementation of AwaitListPrefMatches.
class ListPrefMatchChecker : public PrefMatchChecker {
 public:
  explicit ListPrefMatchChecker(const char* path);
  ~ListPrefMatchChecker() override;

  // Implementation of PrefMatchChecker.
  bool IsExitConditionSatisfied() override;
};

ListPrefMatchChecker::ListPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {
}

ListPrefMatchChecker::~ListPrefMatchChecker() {
}

bool ListPrefMatchChecker::IsExitConditionSatisfied() {
  return ListPrefMatches(GetPath());
}

// Helper class used in the implementation of AwaitBooleanPrefMatches.
class BooleanPrefMatchChecker : public PrefMatchChecker {
 public:
  explicit BooleanPrefMatchChecker(const char* path);
  ~BooleanPrefMatchChecker() override;

  // Implementation of PrefMatchChecker.
  bool IsExitConditionSatisfied() override;
};

BooleanPrefMatchChecker::BooleanPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {
}

BooleanPrefMatchChecker::~BooleanPrefMatchChecker() {
}

bool BooleanPrefMatchChecker::IsExitConditionSatisfied() {
  return BooleanPrefMatches(GetPath());
}

// Helper class used in the implementation of AwaitIntegerPrefMatches.
class IntegerPrefMatchChecker : public PrefMatchChecker {
 public:
  explicit IntegerPrefMatchChecker(const char* path);
  ~IntegerPrefMatchChecker() override;

  // Implementation of PrefMatchChecker.
  bool IsExitConditionSatisfied() override;
};

IntegerPrefMatchChecker::IntegerPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {
}

IntegerPrefMatchChecker::~IntegerPrefMatchChecker() {
}

bool IntegerPrefMatchChecker::IsExitConditionSatisfied() {
  return IntegerPrefMatches(GetPath());
}

// Helper class used in the implementation of AwaitStringPrefMatches.
class StringPrefMatchChecker : public PrefMatchChecker {
 public:
  explicit StringPrefMatchChecker(const char* path);
  ~StringPrefMatchChecker() override;

  // Implementation of PrefMatchChecker.
  bool IsExitConditionSatisfied() override;
};

StringPrefMatchChecker::StringPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {
}

StringPrefMatchChecker::~StringPrefMatchChecker() {
}

bool StringPrefMatchChecker::IsExitConditionSatisfied() {
  return StringPrefMatches(GetPath());
}

}  //  namespace

bool AwaitListPrefMatches(const char* pref_name) {
  ListPrefMatchChecker checker(pref_name);
  checker.Wait();
  return !checker.TimedOut();
}

bool AwaitBooleanPrefMatches(const char* pref_name) {
  BooleanPrefMatchChecker checker(pref_name);
  checker.Wait();
  return !checker.TimedOut();
}

bool AwaitIntegerPrefMatches(const char* pref_name) {
  IntegerPrefMatchChecker checker(pref_name);
  checker.Wait();
  return !checker.TimedOut();
}

bool AwaitStringPrefMatches(const char* pref_name) {
  StringPrefMatchChecker checker(pref_name);
  checker.Wait();
  return !checker.TimedOut();
}

}  // namespace preferences_helper
