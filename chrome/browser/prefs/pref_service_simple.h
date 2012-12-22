// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_SIMPLE_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_SIMPLE_H_

#include "chrome/browser/prefs/pref_service.h"

// A simple PrefService implementation.
class PrefServiceSimple : public PrefService {
 public:
  // You may wish to use PrefServiceBuilder or one of its subclasses
  // for simplified construction.
  PrefServiceSimple(
      PrefNotifierImpl* pref_notifier,
      PrefValueStore* pref_value_store,
      PersistentPrefStore* user_prefs,
      DefaultPrefStore* default_store,
      base::Callback<void(PersistentPrefStore::PrefReadError)>
          read_error_callback,
      bool async);
  virtual ~PrefServiceSimple();

  void RegisterBooleanPref(const char* path, bool default_value);
  void RegisterIntegerPref(const char* path, int default_value);
  void RegisterDoublePref(const char* path, double default_value);
  void RegisterStringPref(const char* path, const std::string& default_value);
  void RegisterFilePathPref(const char* path, const FilePath& default_value);
  void RegisterListPref(const char* path);
  void RegisterDictionaryPref(const char* path);
  void RegisterListPref(const char* path, base::ListValue* default_value);
  void RegisterDictionaryPref(
      const char* path, base::DictionaryValue* default_value);
  void RegisterInt64Pref(const char* path,
                         int64 default_value);

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefServiceSimple);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_SIMPLE_H_
