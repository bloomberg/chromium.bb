// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REGISTRY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/observed_entry.h"
#include "chrome/browser/chromeos/file_system_provider/registry_interface.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace chromeos {
namespace file_system_provider {

// Key names for preferences.
extern const char kPrefKeyFileSystemId[];
extern const char kPrefKeyDisplayName[];
extern const char kPrefKeyWritable[];
extern const char kPrefKeySupportsNotifyTag[];
extern const char kPrefKeyObservedEntries[];
extern const char kPrefKeyObservedEntryEntryPath[];
extern const char kPrefKeyObservedEntryRecursive[];
extern const char kPrefKeyObservedEntryPersistentOrigins[];
extern const char kPrefKeyObservedEntryLastTag[];

class ProvidedFileSystemInfo;

// Registers preferences to remember registered file systems between reboots.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Remembers and restores file systems in a persistent storage.
class Registry : public RegistryInterface {
 public:
  explicit Registry(Profile* profile);
  virtual ~Registry();

  // RegistryInterface overrides.
  virtual void RememberFileSystem(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntries& observed_entries) override;
  virtual void ForgetFileSystem(const std::string& extension_id,
                                const std::string& file_system_id) override;
  virtual scoped_ptr<RestoredFileSystems> RestoreFileSystems(
      const std::string& extension_id) override;
  virtual void UpdateObservedEntryTag(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntry& observed_entry) override;

 private:
  Profile* profile_;  // Not owned.
  DISALLOW_COPY_AND_ASSIGN(Registry);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REGISTRY_H_
