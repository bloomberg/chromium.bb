// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TESTING_PREF_SERVICE_H_
#define CHROME_TEST_TESTING_PREF_SERVICE_H_
#pragma once

#include <chrome/browser/pref_service.h>

// A PrefService subclass for testing. It operates totally in memory and
// provides additional API for manipulating preferences at the different levels
// (managed, extension, user) conveniently.
class TestingPrefService : public PrefService {
 public:
  // Create an empty instance.
  TestingPrefService();

  // Read the value of a preference from the managed layer. Returns NULL if the
  // preference is not defined at the managed layer.
  const Value* GetManagedPref(const wchar_t* path);

  // Set a preference on the managed layer and fire observers if the preference
  // changed. Assumes ownership of |value|.
  void SetManagedPref(const wchar_t* path, Value* value);

  // Clear the preference on the managed layer and fire observers if the
  // preference has been defined previously.
  void RemoveManagedPref(const wchar_t* path);

  // Similar to the above, but for user preferences.
  const Value* GetUserPref(const wchar_t* path);
  void SetUserPref(const wchar_t* path, Value* value);
  void RemoveUserPref(const wchar_t* path);

 private:
  // Reads the value of the preference indicated by |path| from |pref_store|.
  // Returns NULL if the preference was not found.
  const Value* GetPref(PrefStore* pref_store, const wchar_t* path);

  // Sets the value for |path| in |pref_store|.
  void SetPref(PrefStore* pref_store, const wchar_t* path, Value* value);

  // Removes the preference identified by |path| from |pref_store|.
  void RemovePref(PrefStore* pref_store, const wchar_t* path);

  // Pointers to the pref stores our value store uses.
  PrefStore* managed_prefs_;
  PrefStore* user_prefs_;
};

#endif  // CHROME_TEST_TESTING_PREF_SERVICE_H_
