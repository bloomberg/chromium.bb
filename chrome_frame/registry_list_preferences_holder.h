// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A utility class for accessing and caching registry preferences that take
// the form of lists of strings.
// TODO(robertshield): Use the RegistryWatcher stuff to keep this list up to
// date.

#ifndef CHROME_FRAME_REGISTRY_LIST_PREFERENCES_HOLDER_H_
#define CHROME_FRAME_REGISTRY_LIST_PREFERENCES_HOLDER_H_
#pragma once

#include <windows.h>

#include <vector>

#include "base/string16.h"

class RegistryListPreferencesHolder {
 public:
  RegistryListPreferencesHolder();

  // Initializes the RegistryListPreferencesHolder using the values stored
  // under the key |list_name| stored at |registry_path| under |hive|.
  void Init(HKEY hive,
            const wchar_t* registry_path,
            const wchar_t* list_name);

  bool Valid() const { return valid_; }

  // Returns true iff |string| matches any of the strings in values_, using the
  // matching logic in base's MatchPattern().
  bool ListMatches(const string16& string) const;

  // Manually add a string to values_ for testing purposes.
  void AddStringForTesting(const string16& string);

  // Reset the holder causing it to be re-initialized, for testing purposes
  // only.
  // TODO(robertshield): Remove this once the RegistryWatcher stuff is wired up.
  void ResetForTesting();

 private:
  std::vector<string16> values_;
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(RegistryListPreferencesHolder);
};

#endif  // CHROME_FRAME_REGISTRY_LIST_PREFERENCES_HOLDER_H_
