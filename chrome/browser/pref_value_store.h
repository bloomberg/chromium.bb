// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREF_VALUE_STORE_H_
#define CHROME_BROWSER_PREF_VALUE_STORE_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/string16.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/pref_store.h"

class PrefStore;

// The class PrefValueStore provides values for preferences. Each Preference
// has a unique name. This name is used to retrieve the value of a Preference.
// The value of a preference can be either managed, user-defined or recommended.
// Managed preference values are set (managed) by a third person (like an
// admin for example). They have the highest priority and can not be
// altered by the user.
// User-defined values are chosen by the user. If there is already
// a managed value for a preference the user-defined value is ignored and
// the managed value is used (returned).
// Otherwise user-defined values have a higher precedence than recommended
// values. Recommended preference values are set by a third person
// (like an admin).
class PrefValueStore {
 public:
  // In decreasing order of precedence:
  //   |managed_prefs| contains all managed (policy) preference values.
  //   |extension_prefs| contains preference values set by extensions.
  //   |user_prefs| contains all user-set preference values.
  //   |recommended_prefs| contains all recommended (policy) preference values.
  PrefValueStore(PrefStore* managed_prefs,
                 PrefStore* extension_prefs,
                 PrefStore* user_prefs,
                 PrefStore* recommended_prefs);

  ~PrefValueStore();

  // Get the preference value for the given preference name.
  // Return true if a value for the given preference name was found.
  bool GetValue(const std::wstring& name, Value** out_value) const;

  // Read preference values into the three PrefStores so that they are available
  // through the GetValue method. Return the first error that occurs (but
  // continue reading the remaining PrefStores).
  PrefStore::PrefReadError ReadPrefs();

  // Persists prefs (to disk or elsewhere). Returns true if writing values was
  // successful. In practice, only the user prefs are expected to be written
  // out.
  bool WritePrefs();

  // Calls the method ScheduleWritePrefs on the PrefStores. In practice, only
  // the user prefs are expected to be written out.
  void ScheduleWritePrefs();

  // Returns true if the PrefValueStore contains the given preference.
  bool HasPrefPath(const wchar_t* name) const;

  // Returns true if the PrefValueStore is read-only.
  // Because the managed and recommended PrefStores are always read-only, the
  // PrefValueStore as a whole is read-only if the PrefStore containing the user
  // preferences is read-only.
  bool ReadOnly();

  // Alters the user-defined value of a preference. Even if the preference is
  // managed this method allows the user-defined value of the preference to be
  // set. But GetValue calls will not return this value as long as the
  // preference is managed. Instead GetValue will return the managed value
  // of the preference. Note that the PrefValueStore takes the ownership of
  // the value referenced by |in_value|. It is an error to call this when no
  // user PrefStore has been set.
  void SetUserPrefValue(const wchar_t* name, Value* in_value);

  // Removes a value from the PrefValueStore. If a preference is managed
  // or recommended this function should have no effect.
  void RemoveUserPrefValue(const wchar_t* name);

  // Returns true if the preference with the given name is managed.
  // A preference is managed if a managed value is available for that
  // preference.
  bool PrefValueIsManaged(const wchar_t* name);

 private:
  // PrefStores must be listed here in order from highest to lowest priority.
  enum PrefStoreType {
    MANAGED = 0,
    EXTENSION,
    USER,
    RECOMMENDED,
    PREF_STORE_TYPE_MAX = RECOMMENDED
  };

  scoped_ptr<PrefStore> pref_stores_[PREF_STORE_TYPE_MAX + 1];

  DISALLOW_COPY_AND_ASSIGN(PrefValueStore);
};

#endif  // CHROME_BROWSER_PREF_VALUE_STORE_H_
