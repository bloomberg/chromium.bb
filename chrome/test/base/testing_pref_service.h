// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PREF_SERVICE_H_
#define CHROME_TEST_BASE_TESTING_PREF_SERVICE_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "chrome/browser/prefs/pref_service.h"

class DefaultPrefStore;
class PrefModelAssociator;
class PrefNotifierImpl;
class TestingBrowserProcess;
class TestingPrefStore;

// A PrefService subclass for testing. It operates totally in memory and
// provides additional API for manipulating preferences at the different levels
// (managed, extension, user) conveniently.
class TestingPrefServiceBase : public PrefService {
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
      TestingPrefStore* managed_platform_prefs,
      TestingPrefStore* user_prefs,
      TestingPrefStore* recommended_platform_prefs,
      DefaultPrefStore* default_store,
      PrefModelAssociator* pref_sync_associator,
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
  scoped_refptr<TestingPrefStore> managed_platform_prefs_;
  scoped_refptr<TestingPrefStore> user_prefs_;
  scoped_refptr<TestingPrefStore> recommended_platform_prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestingPrefServiceBase);
};

// Class for simplified construction of TestPrefServiceBase objects.
class TestingPrefService : public TestingPrefServiceBase {
 public:
  TestingPrefService();
  virtual ~TestingPrefService();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingPrefService);
};

// Helper class to temporarily set up a |local_state| in the global
// TestingBrowserProcess (for most unit tests it's NULL).
class ScopedTestingLocalState {
 public:
  explicit ScopedTestingLocalState(TestingBrowserProcess* browser_process);
  ~ScopedTestingLocalState();

  TestingPrefService* Get() {
    return &local_state_;
  }

 private:
  TestingBrowserProcess* browser_process_;
  TestingPrefService local_state_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTestingLocalState);
};

#endif  // CHROME_TEST_BASE_TESTING_PREF_SERVICE_H_
