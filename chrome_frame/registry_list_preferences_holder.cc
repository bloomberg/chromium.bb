// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/registry_list_preferences_holder.h"

#include "base/string_util.h"
#include "base/win/registry.h"

RegistryListPreferencesHolder::RegistryListPreferencesHolder() : valid_(false) {
}

void RegistryListPreferencesHolder::Init(HKEY hive,
                                         const wchar_t* registry_path,
                                         const wchar_t* list_name) {
  string16 list_path(registry_path);
  list_path += L"\\";
  list_path += list_name;
  base::win::RegistryValueIterator string_list(hive, list_path.c_str());
  for (; string_list.Valid(); ++string_list)
    values_.push_back(string_list.Name());

  valid_ = true;
}

bool RegistryListPreferencesHolder::ListMatches(const string16& string) const {
  DCHECK(Valid());
  std::vector<string16>::const_iterator iter(values_.begin());
  for (; iter != values_.end(); ++iter) {
    if (MatchPattern(string, *iter))
      return true;
  }

  return false;
}

void RegistryListPreferencesHolder::AddStringForTesting(
    const string16& string) {
  values_.push_back(string);
}

void RegistryListPreferencesHolder::ResetForTesting() {
  values_.clear();
  valid_ = false;
}
