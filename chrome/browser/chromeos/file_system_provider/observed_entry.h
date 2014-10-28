// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OBSERVED_ENTRY_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OBSERVED_ENTRY_H_

#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "url/gurl.h"

namespace chromeos {
namespace file_system_provider {

struct ObservedEntry;
struct Subscriber;

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

// Map of subscribers for notifications about an observed entry.
typedef std::map<GURL, Subscriber> Subscribers;

// Represents a subscriber for notification about an observed entry. There may
// be up to one subscriber per origin for the same observed entry.
struct Subscriber {
  Subscriber();
  ~Subscriber();

  // Origin of the subscriber.
  GURL origin;

  // Whether the subscriber should be restored after shutdown or not.
  bool persistent;
};

// Represents an observed entry on a file system.
struct ObservedEntry {
  ObservedEntry();
  ~ObservedEntry();

  // Map of subscribers for notifications of the observed entry.
  Subscribers subscribers;

  // Path of the observed entry.
  base::FilePath entry_path;

  // Whether observing is recursive or not.
  bool recursive;

  // Tag of the last notification for this observed entry. May be empty if not
  // supported.
  std::string last_tag;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OBSERVED_ENTRY_H_
