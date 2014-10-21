// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REGISTRY_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REGISTRY_INTERFACE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/observed_entry.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"

namespace chromeos {
namespace file_system_provider {

// Remembers and restores file systems in a persistent storage.
class RegistryInterface {
 public:
  struct RestoredFileSystem;

  // List of file systems together with their observed entries to be remounted.
  typedef std::vector<RestoredFileSystem> RestoredFileSystems;

  // Information about a file system to be restored.
  struct RestoredFileSystem {
    RestoredFileSystem();
    ~RestoredFileSystem();

    std::string extension_id;
    MountOptions options;
    ObservedEntries observed_entries;
  };

  virtual ~RegistryInterface();

  // Remembers the file system in preferences, in order to remount after a
  // reboot.
  virtual void RememberFileSystem(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntries& observed_entries) = 0;

  // Removes the file system from preferences, so it is not remounmted anymore
  // after a reboot.
  virtual void ForgetFileSystem(const std::string& extension_id,
                                const std::string& file_system_id) = 0;

  // Restores from preferences file systems mounted previously by the
  // |extension_id| providing extension. The returned list should be used to
  // remount them.
  virtual scoped_ptr<RestoredFileSystems> RestoreFileSystems(
      const std::string& extension_id) = 0;

  // Updates a tag for the specified observed entry.
  virtual void UpdateObservedEntryTag(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntry& observed_entry) = 0;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REGISTRY_INTERFACE_H_
