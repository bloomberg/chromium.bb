// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IE_IMPORTER_TEST_REGISTRY_OVERRIDER_WIN_H_
#define CHROME_BROWSER_IMPORTER_IE_IMPORTER_TEST_REGISTRY_OVERRIDER_WIN_H_

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"

// A helper class to override HKEY_CURRENT_USER to point to a test key from this
// process' point of view. Uses a random guid as a subkey for the override in
// order for multiple tests to be able to use this functionality in parallel.
// Also provides functionality (via an environment variable) to let child
// processes check whether they should also override. This class is thread-safe.
// Always unsets any existing override upon being deleted.
class IEImporterTestRegistryOverrider {
 public:
  IEImporterTestRegistryOverrider();

  // Unsets any existing registry override and deletes the temporary registry
  // key if |override_initiated_|.
  ~IEImporterTestRegistryOverrider();

  // Calls StartRegistryOverride() and sets an environment variable so that
  // future calls to StartRegistryOverrideIfNeeded() from child processes also
  // result in an overriden HKEY_CURRENT_USER hive. This method should only be
  // called once across all processes.
  // Returns false on unexpected failure.
  bool SetRegistryOverride();

  // Calls StartRegistryOverride() if the environment was set by a parent
  // process via SetRegistryOverride().
  // Returns false on unexpected failure.
  bool StartRegistryOverrideIfNeeded();

 private:
  // Overrides HKCU to point to a test key (ending with |subkey|) from this
  // process' point of view. Re-applies the override without taking ownership if
  // |override_active_in_process_|.
  // Returns false on unexpected failure.
  bool StartRegistryOverride(const base::string16& subkey);

  // Reads the environment variable set by SetRegistryOverride into |subkey| if
  // it exists. Returns true if the variable was successfully read.
  bool GetSubKeyFromEnvironment(base::string16* subkey);

  // Whether this object started an override for the current process (and thus
  // will be responsible for unsetting it when done).
  bool override_performed_;

  // Whether this object is the one that initiated the override via
  // SetRegistryOverride() (and thus will be the one responsible for deleting
  // the temporary key when done).
  bool override_initiated_;

  // Whether a registry override was initiated in the current process.
  static bool override_active_in_process_;

  // This lock should be held before accessing this class' static data and any
  // of its objects' members.
  static base::LazyInstance<base::Lock>::Leaky lock_;

  DISALLOW_COPY_AND_ASSIGN(IEImporterTestRegistryOverrider);
};

#endif  // CHROME_BROWSER_IMPORTER_IE_IMPORTER_TEST_REGISTRY_OVERRIDER_WIN_H_
