// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_REGISTRY_OVERRIDE_MANAGER_H_
#define CHROME_CHROME_CLEANER_OS_REGISTRY_OVERRIDE_MANAGER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"

namespace base {
class CommandLine;
}

namespace chrome_cleaner {

// Allows a test to easily override the registry so that it can start from a
// known good state, or make sure to not leave any side effects once the test
// completes. All the overrides are scoped to the lifetime of the override
// manager.
//
// The override can be enabled by calling CreateAndUseTemporaryRegistry().
// Temporary keys will be created to override the predefined keys
// (HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, ...). Alternatively, a process can
// share the temporary keys created by another process. To do so, create a
// temporary registry on a first process, using CreateTemporaryRegistry() or
// CreateAndUseTemporaryRegistry(). Then, obtain the id of the created temporary
// registry by calling temp_registry_id() and provide this value to
// UseTemporaryRegistry() on the second process. The temporary registry is owned
// by the process that created it.
//
// The temporary registry is stored under a root key similar to:
//   HKCU\Software\ChromeCleaner\TempTestKeys\
//       13028145911617809$02AB211C-CF73-478D-8D91-618E11998AED
// The key path are comprises of:
//   - The test key root, HKCU\Software\ChromeCleaner\TempTestKeys\
//   - The base::Time::ToDeltaSinceWindowsEpoch().InNanoseconds() of the
//     creation time. This is used to delete stale keys left over from crashed
//     tests.
//   - A GUID used for preventing name collisions (although unlikely) between
//     two RegistryOverrideManagers created with the same timestamp.
//
// This class is inspired from base (registry_util::RegistryOverrideManager),
// but adds the possibility to share a temporary registry between processes.
class RegistryOverrideManager {
 public:
  // Description of a key to clone to the temporary registy.
  struct RegistryKeyToClone {
    // Root key for the key to clone.
    HKEY root;

    // Path of the key to clone, relative to |root|.
    base::string16 path;

    // Indicates whether subkeys should also be cloned.
    bool recursive;
  };
  typedef std::vector<RegistryKeyToClone> RegistryKeyVector;

  RegistryOverrideManager();
  ~RegistryOverrideManager();

  // Create a temporary registry that can be used by other processes.
  // |keys_to_clone| is a vector of keys that will be cloned from the real to
  // the temporary registry, without their security attributes. Return true
  // if all keys from |keys_to_clone| have been cloned.
  bool CreateTemporaryRegistry(const RegistryKeyVector& keys_to_clone);

  // Map all predefined keys to new temporary keys. |keys_to_clone| is a vector
  // of keys that will be cloned from the real to the temporary registry,
  // without their security attributes. Return true if all predefined keys have
  // been mapped to temporary keys and all keys from |keys_to_clone| have been
  // cloned.
  bool CreateAndUseTemporaryRegistry(const RegistryKeyVector& keys_to_clone);

  // Map all predefined keys to temporary keys created by another process.
  // |temp_registry_parent| and |temp_registry_id| are registry identifiers
  // used on another process. Return true if all predefined keys have been
  // mapped to temporary keys.
  bool UseTemporaryRegistry(const base::string16& temp_registry_parent,
                            const base::string16& temp_registry_id);

  void OverrideRegistryDuringTests(base::CommandLine* command_line);

  // Return the temporary registry identifier, or an empty string if the
  //    registry override is not enabled.
  base::string16 temp_registry_id() const { return temp_registry_id_; }

  // Path to the temporary registry key.
  base::string16 GetTempRegistryPath() const;

  // Parse the temporary registry path into the parent key path and the
  // identifier of the temporary registry.
  static void ParseTempRegistryPath(const base::string16& temp_registry_path,
                                    base::string16* temp_registry_parent,
                                    base::string16* temp_registry_id);

 private:
  friend class RegistryOverrideManagerTest;
  FRIEND_TEST_ALL_PREFIXES(RegistryOverrideManagerTest,
                           CreateTemporaryRegistry);

  // Used for testing only.
  RegistryOverrideManager(const base::Time& timestamp,
                          const base::string16& temp_registry_parent);

  bool UseTemporaryRegistryInternal();

  static base::string16 GenerateTempRegistryId(const base::Time& timestamp);

  // Default key under which temporary registries are created.
  static const base::char16 kTempRegistryParent[];

  // Indicates whether the temporary registry should be deleted with this
  // object.
  bool owns_temp_registry_;

  // Identifier of the temporary registry.
  base::string16 temp_registry_id_;

  // The key under which temporary registries are created.
  base::string16 temp_registry_parent_;

  // Active overrides.
  std::vector<HKEY> overrides_;

  DISALLOW_COPY_AND_ASSIGN(RegistryOverrideManager);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_REGISTRY_OVERRIDE_MANAGER_H_
