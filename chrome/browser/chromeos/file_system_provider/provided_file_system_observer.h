// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_OBSERVER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/file_system_provider/observed_entry.h"

namespace chromeos {
namespace file_system_provider {

class ProvidedFileSystemInfo;
class RequestManager;

// Observer class to be notified about changes happened to the provided file
// system, especially observed entries.
class ProvidedFileSystemObserver {
 public:
  struct Change;

  // Type of a change to an observed entry.
  enum ChangeType { CHANGED, DELETED };

  // Lust of changes.
  typedef std::vector<Change> Changes;

  // Describes a change related to an observed directory.
  struct Change {
    Change();
    ~Change();

    base::FilePath entry_path;
    ChangeType change_type;
  };

  // Called when an observed entry is changed, including removals. |callback|
  // *must* be called after the entry change is handled. Once all observers
  // call the callback, the tag will be updated and OnObservedEntryTagUpdated
  // called. The reference to |changes| is valid at least as long as |callback|.
  virtual void OnObservedEntryChanged(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntry& observed_entry,
      ChangeType change_type,
      const Changes& changes,
      const base::Closure& callback) = 0;

  // Called after the tag value is updated for the observed entry.
  virtual void OnObservedEntryTagUpdated(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntry& observed_entry) = 0;

  // Called after the list of observed entries is changed.
  virtual void OnObservedEntryListChanged(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntries& observed_entries) = 0;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_OBSERVER_H_
