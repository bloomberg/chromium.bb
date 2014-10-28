// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/observed_entry.h"

namespace chromeos {
namespace file_system_provider {

ObservedEntryKey::ObservedEntryKey(const base::FilePath& entry_path,
                                   bool recursive)
    : entry_path(entry_path), recursive(recursive) {
}

ObservedEntryKey::~ObservedEntryKey() {
}

bool ObservedEntryKey::Comparator::operator()(const ObservedEntryKey& a,
                                              const ObservedEntryKey& b) const {
  if (a.entry_path != b.entry_path)
    return a.entry_path < b.entry_path;
  return a.recursive < b.recursive;
}

Subscriber::Subscriber() : persistent(false) {
}

Subscriber::~Subscriber() {
}

ObservedEntry::ObservedEntry() : recursive(false) {
}

ObservedEntry::~ObservedEntry() {
}

}  // namespace file_system_provider
}  // namespace chromeos
