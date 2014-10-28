// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OBSERVED_ENTRY_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OBSERVED_ENTRY_H_

#include <map>
#include <string>

#include "base/files/file_path.h"

namespace chromeos {
namespace file_system_provider {

struct ObservedEntry;

// Key for storing an observed entry in the map. There may be two observers
// per path, as long as one is recursive, and the other one not.
struct ObservedEntryKey {
  ObservedEntryKey(const base::FilePath& entry_path, bool recursive);
  ~ObservedEntryKey();

  struct Comparator {
    bool operator()(const ObservedEntryKey& a, const ObservedEntryKey& b) const;
  };

  base::FilePath entry_path;
  bool recursive;
};

// List of observed entries.
typedef std::map<ObservedEntryKey, ObservedEntry, ObservedEntryKey::Comparator>
    ObservedEntries;

// Represents an observed entry on a file system.
struct ObservedEntry {
  ObservedEntry();
  ~ObservedEntry();

  base::FilePath entry_path;
  bool recursive;
  std::string last_tag;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OBSERVED_ENTRY_H_
