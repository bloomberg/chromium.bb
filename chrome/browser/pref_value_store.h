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
  // |managed_prefs| contains all managed preference values. They have the
  // highest priority and precede user-defined preference values. |user_prefs|
  // contains all user-defined preference values. User-defined values precede
  // recommended values. |recommended_prefs| contains all recommended
  // preference values.
  PrefValueStore(PrefStore* managed_prefs,
                 PrefStore* user_prefs,
                 PrefStore* recommended_prefs);

  ~PrefValueStore();

  // Get the preference value for the given preference name.
  // Return true if a value for the given preference name was found.
  bool GetValue(const std::wstring& name, Value** out_value) const;

  // Read preference values into the three PrefStores so that they are available
  // through the GetValue method.
  PrefStore::PrefReadError ReadPrefs();

  // Write user settable preference values. Return true if writing values was
  // successfull.
  bool WritePrefs();

  // Calls the method ScheduleWritePrefs on the PrefStores.
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
  // the value referenced by |in_value|.
  void SetUserPrefValue(const wchar_t* name, Value* in_value);

  // Removes a value from the PrefValueStore. If a preference is managed
  // or recommended this function should have no effect.
  void RemoveUserPrefValue(const wchar_t* name);

  // Returns true if the preference with the given name is managed.
  // A preference is managed if a managed value is available for that
  // preference.
  bool PrefValueIsManaged(const wchar_t* name);

 private:
  scoped_ptr<PrefStore> managed_prefs_;
  scoped_ptr<PrefStore> user_prefs_;
  scoped_ptr<PrefStore> recommended_prefs_;

  DISALLOW_COPY_AND_ASSIGN(PrefValueStore);
};

#endif  // CHROME_BROWSER_PREF_VALUE_STORE_H_
