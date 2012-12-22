// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PREF_SERVICE_H_
#define CHROME_TEST_BASE_TESTING_PREF_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "base/prefs/testing_pref_store.h"
#include "chrome/browser/prefs/pref_service.h"

class DefaultPrefStore;
class PrefModelAssociator;
class PrefNotifierImpl;
class TestingBrowserProcess;
class TestingPrefStore;

// A PrefService subclass for testing. It operates totally in memory and
// provides additional API for manipulating preferences at the different levels
// (managed, extension, user) conveniently.
//
// Use this via its specializations, TestingPrefServiceSimple and
// TestingPrefServiceSyncable.
template <class SuperPrefService>
class TestingPrefServiceBase : public SuperPrefService {
 public:
  virtual ~TestingPrefServiceBase();

  // Read the value of a preference from the managed layer. Returns NULL if the
  // preference is not defined at the managed layer.
  const Value* GetManagedPref(const char* path) const;

  // Set a preference on the managed layer and fire observers if the preference
  // changed. Assumes ownership of |value|.
  void SetManagedPref(const char* path, Value* value);

  // Clear the preference on the managed layer and fire observers if the
  // preference has been defined previously.
  void RemoveManagedPref(const char* path);

  // Similar to the above, but for user preferences.
  const Value* GetUserPref(const char* path) const;
  void SetUserPref(const char* path, Value* value);
  void RemoveUserPref(const char* path);

  // Similar to the above, but for recommended policy preferences.
  const Value* GetRecommendedPref(const char* path) const;
  void SetRecommendedPref(const char* path, Value* value);
  void RemoveRecommendedPref(const char* path);

 protected:
  TestingPrefServiceBase(
      TestingPrefStore* managed_prefs,
      TestingPrefStore* user_prefs,
      TestingPrefStore* recommended_prefs,
      DefaultPrefStore* default_store,
      PrefNotifierImpl* pref_notifier);

 private:
  // Reads the value of the preference indicated by |path| from |pref_store|.
  // Returns NULL if the preference was not found.
  const Value* GetPref(TestingPrefStore* pref_store, const char* path) const;

  // Sets the value for |path| in |pref_store|.
  void SetPref(TestingPrefStore* pref_store, const char* path, Value* value);

  // Removes the preference identified by |path| from |pref_store|.
  void RemovePref(TestingPrefStore* pref_store, const char* path);

  // Pointers to the pref stores our value store uses.
  scoped_refptr<TestingPrefStore> managed_prefs_;
  scoped_refptr<TestingPrefStore> user_prefs_;
  scoped_refptr<TestingPrefStore> recommended_prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestingPrefServiceBase);
};

// Test version of PrefServiceSimple.
class TestingPrefServiceSimple
    : public TestingPrefServiceBase<PrefServiceSimple> {
 public:
  TestingPrefServiceSimple();
  virtual ~TestingPrefServiceSimple();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingPrefServiceSimple);
};

// Test version of PrefServiceSyncable.
class TestingPrefServiceSyncable
    : public TestingPrefServiceBase<PrefServiceSyncable> {
 public:
  TestingPrefServiceSyncable();
  virtual ~TestingPrefServiceSyncable();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingPrefServiceSyncable);
};

// Helper class to temporarily set up a |local_state| in the global
// TestingBrowserProcess (for most unit tests it's NULL).
class ScopedTestingLocalState {
 public:
  explicit ScopedTestingLocalState(TestingBrowserProcess* browser_process);
  ~ScopedTestingLocalState();

  TestingPrefServiceSimple* Get() {
    return &local_state_;
  }

 private:
  TestingBrowserProcess* browser_process_;
  TestingPrefServiceSimple local_state_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTestingLocalState);
};

template<>
TestingPrefServiceBase<PrefServiceSimple>::TestingPrefServiceBase(
    TestingPrefStore* managed_prefs,
    TestingPrefStore* user_prefs,
    TestingPrefStore* recommended_prefs,
    DefaultPrefStore* default_store,
    PrefNotifierImpl* pref_notifier);

template<>
TestingPrefServiceBase<PrefServiceSyncable>::TestingPrefServiceBase(
    TestingPrefStore* managed_prefs,
    TestingPrefStore* user_prefs,
    TestingPrefStore* recommended_prefs,
    DefaultPrefStore* default_store,
    PrefNotifierImpl* pref_notifier);

template<class SuperPrefService>
TestingPrefServiceBase<SuperPrefService>::~TestingPrefServiceBase() {
}

template<class SuperPrefService>
const Value* TestingPrefServiceBase<SuperPrefService>::GetManagedPref(
    const char* path) const {
  return GetPref(managed_prefs_, path);
}

template<class SuperPrefService>
void TestingPrefServiceBase<SuperPrefService>::SetManagedPref(
    const char* path, Value* value) {
  SetPref(managed_prefs_, path, value);
}

template<class SuperPrefService>
void TestingPrefServiceBase<SuperPrefService>::RemoveManagedPref(
    const char* path) {
  RemovePref(managed_prefs_, path);
}

template<class SuperPrefService>
const Value* TestingPrefServiceBase<SuperPrefService>::GetUserPref(
    const char* path) const {
  return GetPref(user_prefs_, path);
}

template<class SuperPrefService>
void TestingPrefServiceBase<SuperPrefService>::SetUserPref(
    const char* path, Value* value) {
  SetPref(user_prefs_, path, value);
}

template<class SuperPrefService>
void TestingPrefServiceBase<SuperPrefService>::RemoveUserPref(
    const char* path) {
  RemovePref(user_prefs_, path);
}

template<class SuperPrefService>
const Value* TestingPrefServiceBase<SuperPrefService>::GetRecommendedPref(
    const char* path) const {
  return GetPref(recommended_prefs_, path);
}

template<class SuperPrefService>
void TestingPrefServiceBase<SuperPrefService>::SetRecommendedPref(
    const char* path, Value* value) {
  SetPref(recommended_prefs_, path, value);
}

template<class SuperPrefService>
void TestingPrefServiceBase<SuperPrefService>::RemoveRecommendedPref(
    const char* path) {
  RemovePref(recommended_prefs_, path);
}

template<class SuperPrefService>
const Value* TestingPrefServiceBase<SuperPrefService>::GetPref(
    TestingPrefStore* pref_store, const char* path) const {
  const Value* res;
  return pref_store->GetValue(path, &res) ? res : NULL;
}

template<class SuperPrefService>
void TestingPrefServiceBase<SuperPrefService>::SetPref(
    TestingPrefStore* pref_store, const char* path, Value* value) {
  pref_store->SetValue(path, value);
}

template<class SuperPrefService>
void TestingPrefServiceBase<SuperPrefService>::RemovePref(
    TestingPrefStore* pref_store, const char* path) {
  pref_store->RemoveValue(path);
}

#endif  // CHROME_TEST_BASE_TESTING_PREF_SERVICE_H_
