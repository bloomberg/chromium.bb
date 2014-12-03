// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_PREF_REGISTRY_SIMPLE_H_
#define BASE_PREFS_PREF_REGISTRY_SIMPLE_H_

#include <string>

#include "base/prefs/base_prefs_export.h"
#include "base/prefs/pref_registry.h"

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
}

// A simple implementation of PrefRegistry.
class BASE_PREFS_EXPORT PrefRegistrySimple : public PrefRegistry {
 public:
  PrefRegistrySimple();

  void RegisterBooleanPref(const std::string& path, bool default_value);
  void RegisterIntegerPref(const std::string& path, int default_value);
  void RegisterDoublePref(const std::string& path, double default_value);
  void RegisterStringPref(const std::string& path,
                          const std::string& default_value);
  void RegisterFilePathPref(const std::string& path,
                            const base::FilePath& default_value);
  void RegisterListPref(const std::string& path);
  void RegisterDictionaryPref(const std::string& path);
  void RegisterListPref(const std::string& path,
                        base::ListValue* default_value);
  void RegisterDictionaryPref(const std::string& path,
                              base::DictionaryValue* default_value);
  void RegisterInt64Pref(const std::string& path, int64 default_value);

 private:
  ~PrefRegistrySimple() override;

  DISALLOW_COPY_AND_ASSIGN(PrefRegistrySimple);
};

#endif  // BASE_PREFS_PREF_REGISTRY_SIMPLE_H_
