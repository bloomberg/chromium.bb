// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TESTING_PREF_SERVICE_H_
#define CHROME_TEST_TESTING_PREF_SERVICE_H_
#pragma once

#include "chrome/browser/prefs/pref_service.h"

class PrefStore;

// A PrefService subclass for testing. It operates totally in memory and
// provides additional API for manipulating preferences at the different levels
// (managed, extension, user) conveniently.
class TestingPrefService : public PrefService {
 public:
  // Subclass to allow directly setting PrefStores.
  class TestingPrefValueStore : public PrefValueStore {
   public:
    TestingPrefValueStore(PrefStore* managed_prefs,
                          PrefStore* extension_prefs,
                          PrefStore* command_line_prefs,
                          PrefStore* user_prefs,
                          PrefStore* recommended_prefs);
  };

  // Create an empty instance.
  TestingPrefService();

  // Read the value of a preference from the managed layer. Returns NULL if the
  // preference is not defined at the managed layer.
  const Value* GetManagedPref(const char* path);

  // Set a preference on the managed layer and fire observers if the preference
  // changed. Assumes ownership of |value|.
  void SetManagedPref(const char* path, Value* value);

  // Clear the preference on the managed layer and fire observers if the
  // preference has been defined previously.
  void RemoveManagedPref(const char* path);

  // Similar to the above, but for user preferences.
  const Value* GetUserPref(const char* path);
  void SetUserPref(const char* path, Value* value);
  void RemoveUserPref(const char* path);

 private:
  // Reads the value of the preference indicated by |path| from |pref_store|.
  // Returns NULL if the preference was not found.
  const Value* GetPref(PrefStore* pref_store, const char* path);

  // Sets the value for |path| in |pref_store|.
  void SetPref(PrefStore* pref_store, const char* path, Value* value);

  // Removes the preference identified by |path| from |pref_store|.
  void RemovePref(PrefStore* pref_store, const char* path);

  // Pointers to the pref stores our value store uses.
  PrefStore* managed_prefs_;
  PrefStore* user_prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestingPrefService);
};

#endif  // CHROME_TEST_TESTING_PREF_SERVICE_H_
