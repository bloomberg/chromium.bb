// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TESTING_PREF_SERVICE_H_
#define CHROME_TEST_TESTING_PREF_SERVICE_H_
#pragma once

#include "chrome/browser/prefs/pref_service.h"

class CommandLine;
namespace policy {
class ConfigurationPolicyProvider;
}
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
                          PrefStore* device_management_prefs,
                          PrefStore* extension_prefs,
                          PrefStore* command_line_prefs,
                          PrefStore* user_prefs,
                          PrefStore* recommended_prefs,
                          PrefStore* default_prefs);
  };

  // Create an empty instance.
  TestingPrefService();

  // Create an instance that has a managed PrefStore and a command- line
  // PrefStore. |managed_platform_provider| contains the provider with which to
  // initialize the managed platform PrefStore. If it is NULL, then a
  // DummyPrefStore will be created. |device_management_provider| contains the
  // provider with which to initialize the device management
  // PrefStore. |command_line| contains the provider with which to initialize
  // the command line PrefStore. If it is NULL then a DummyPrefStore will be
  // created as the command line PrefStore.
  TestingPrefService(
      policy::ConfigurationPolicyProvider* managed_platform_provider,
      policy::ConfigurationPolicyProvider* device_management_provider,
      CommandLine* command_line);

  // Read the value of a preference from the managed layer. Returns NULL if the
  // preference is not defined at the managed layer.
  const Value* GetManagedPref(const char* path);

  // Set a preference on the managed layer and fire observers if the preference
  // changed. Assumes ownership of |value|.
  void SetManagedPref(const char* path, Value* value);

  // Clear the preference on the managed layer and fire observers if the
  // preference has been defined previously.
  void RemoveManagedPref(const char* path);

  // Set a preference on the managed layer.  Assumes ownership of |value|.
  // We don't fire observers for each change because in the real code, the
  // notification is sent after all the prefs have been loaded.  See
  // ConfigurationPolicyPrefStore::ReadPrefs().
  void SetManagedPrefWithoutNotification(const char* path, Value* value);

  // Clear the preference on the managed layer.
  // We don't fire observers for each change because in the real code, the
  // notification is sent after all the prefs have been loaded.  See
  // ConfigurationPolicyPrefStore::ReadPrefs().
  void RemoveManagedPrefWithoutNotification(const char* path);

  // Similar to the above, but for user preferences.
  const Value* GetUserPref(const char* path);
  void SetUserPref(const char* path, Value* value);
  void RemoveUserPref(const char* path);

 private:
  // Creates a ConfigurationPolicyPrefStore based on the provided
  // |provider| or a DummyPrefStore if |provider| is NULL.
  PrefStore* CreatePolicyPrefStoreFromProvider(
      policy::ConfigurationPolicyProvider* provider);

  // Creates a CommandLinePrefStore based on the supplied
  // |command_line| or a DummyPrefStore if |command_line| is NULL.
  PrefStore* CreateCommandLinePrefStore(CommandLine* command_line);

  // Reads the value of the preference indicated by |path| from |pref_store|.
  // Returns NULL if the preference was not found.
  const Value* GetPref(PrefStore* pref_store, const char* path);

  // Sets the value for |path| in |pref_store|.
  void SetPref(PrefStore* pref_store, const char* path, Value* value);

  // Removes the preference identified by |path| from |pref_store|.
  void RemovePref(PrefStore* pref_store, const char* path);

  // Pointers to the pref stores our value store uses.
  PrefStore* managed_platform_prefs_;  // weak
  PrefStore* device_management_prefs_;  // weak
  PrefStore* user_prefs_;  // weak
  PrefStore* default_prefs_;  // weak

  DISALLOW_COPY_AND_ASSIGN(TestingPrefService);
};

#endif  // CHROME_TEST_TESTING_PREF_SERVICE_H_
