// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/preferences_helper.h"

#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

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

void ChangeInt64Pref(int index, const char* pref_name, int64 new_value) {
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

void AppendStringPref(int index,
                      const char* pref_name,
                      const std::string& append_value) {
  ChangeStringPref(index,
                   pref_name,
                   GetPrefs(index)->GetString(pref_name) + append_value);
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
      LOG(ERROR) << "Boolean preference " << pref_name << " mismatched in"
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
      LOG(ERROR) << "Integer preference " << pref_name << " mismatched in"
                 << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool Int64PrefMatches(const char* pref_name) {
  int64 reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetInt64(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetInt64(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetInt64(pref_name)) {
      LOG(ERROR) << "Integer preference " << pref_name << " mismatched in"
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
      LOG(ERROR) << "Double preference " << pref_name << " mismatched in"
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
      LOG(ERROR) << "String preference " << pref_name << " mismatched in"
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
      LOG(ERROR) << "base::FilePath preference " << pref_name
                 << " mismatched in" << " profile " << i << ".";
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
      LOG(ERROR) << "List preference " << pref_name << " mismatched in"
                 << " profile " << i << ".";
      return false;
    }
  }
  return true;
}


namespace {

// Helper class used in the implementation of AwaitListPrefMatches.
class ListPrefMatchStatusChecker : public MultiClientStatusChangeChecker {
 public:
  explicit ListPrefMatchStatusChecker(const char* pref_name);
  virtual ~ListPrefMatchStatusChecker();

  virtual bool IsExitConditionSatisfied() OVERRIDE;
  virtual std::string GetDebugMessage() const OVERRIDE;
 private:
  const char* pref_name_;
};

ListPrefMatchStatusChecker::ListPrefMatchStatusChecker(const char* pref_name)
    : MultiClientStatusChangeChecker(
        sync_datatype_helper::test()->GetSyncServices()),
      pref_name_(pref_name) {}

ListPrefMatchStatusChecker::~ListPrefMatchStatusChecker() {}

bool ListPrefMatchStatusChecker::IsExitConditionSatisfied() {
  return ListPrefMatches(pref_name_);
}

std::string ListPrefMatchStatusChecker::GetDebugMessage() const {
  return "Waiting for matching preferences";
}

}  //  namespace

bool AwaitListPrefMatches(const char* pref_name) {
  ListPrefMatchStatusChecker checker(pref_name);
  checker.Wait();
  return !checker.TimedOut();
}

}  // namespace preferences_helper
