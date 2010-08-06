// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREF_VALUE_STORE_H_
#define CHROME_BROWSER_PREF_VALUE_STORE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/pref_store.h"

class PrefStore;
class Profile;

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
// Unless otherwise explicitly noted, all of the methods of this class must
// be called on the UI thread.
class PrefValueStore : public base::RefCountedThreadSafe<PrefValueStore> {
 public:
  // Returns a new PrefValueStore with all applicable PrefStores. The
  // |pref_filename| points to the user preference file. The |profile| is the
  // one to which these preferences apply; it may be NULL if we're dealing
  // with the local state. If |pref_filename| is empty, the user PrefStore will
  // not be created. If |user_only| is true, no PrefStores will be created
  // other than the user PrefStore (if |pref_filename| is not empty). This
  // should not normally be called directly: the usual way to create a
  // PrefValueStore is by creating a PrefService.
  static PrefValueStore* CreatePrefValueStore(const FilePath& pref_filename,
                                              Profile* profile,
                                              bool user_only);

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

  // These methods return true if a preference with the given name is in the
  // indicated pref store, even if that value is currently being overridden by
  // a higher-priority source.
  bool PrefValueInManagedStore(const wchar_t* name);
  bool PrefValueInExtensionStore(const wchar_t* name);
  bool PrefValueInUserStore(const wchar_t* name);

  // These methods return true if a preference with the given name is actually
  // being controlled by the indicated pref store and not being overridden by
  // a higher-priority source.
  bool PrefValueFromExtensionStore(const wchar_t* name);
  bool PrefValueFromUserStore(const wchar_t* name);

  // Check whether a Preference value is modifiable by the user, i.e. whether
  // there is no higher-priority source controlling it.
  bool PrefValueUserModifiable(const wchar_t* name);

  // Signature of callback triggered after policy refresh. Parameter is not
  // passed as reference to prevent passing along a pointer to a set whose
  // lifecycle is managed in another thread.
  typedef Callback1<std::vector<std::string> >::Type AfterRefreshCallback;

  // Called as a result of a notification of policy change. Triggers a
  // reload of managed preferences from policy. Caller must pass in
  // new, uninitialized managed and recommended PrefStores in
  // |managed_pref_store| and |recommended_pref_store| respectively, since
  // PrefValueStore doesn't know about policy-specific PrefStores.
  // |callback| is called with the set of preferences changed by the policy
  // refresh. |callback| is called on the caller's thread as a Task
  // after RefreshPolicyPrefs has returned. RefreshPolicyPrefs takes ownership
  // of the |callback| object.
  void RefreshPolicyPrefs(PrefStore* managed_pref_store,
                          PrefStore* recommended_pref_store,
                          AfterRefreshCallback* callback);

 protected:
  // In decreasing order of precedence:
  //   |managed_prefs| contains all managed (policy) preference values.
  //   |extension_prefs| contains preference values set by extensions.
  //   |command_line_prefs| contains preference values set by command-line
  //        switches.
  //   |user_prefs| contains all user-set preference values.
  //   |recommended_prefs| contains all recommended (policy) preference values.
  //
  // This constructor should only be used internally, or by subclasses in
  // testing. The usual way to create a PrefValueStore is by creating a
  // PrefService.
  PrefValueStore(PrefStore* managed_prefs,
                 PrefStore* extension_prefs,
                 PrefStore* command_line_prefs,
                 PrefStore* user_prefs,
                 PrefStore* recommended_prefs);

 private:
  friend class PrefValueStoreTest;
  FRIEND_TEST_ALL_PREFIXES(PrefValueStoreTest,
                           TestRefreshPolicyPrefsCompletion);

  // PrefStores must be listed here in order from highest to lowest priority.
  enum PrefStoreType {
    // Not associated with an actual PrefStore but used as invalid marker, e.g.
    // as return value.
    INVALID = -1,
    MANAGED = 0,
    EXTENSION,
    COMMAND_LINE,
    USER,
    RECOMMENDED,
    PREF_STORE_TYPE_MAX = RECOMMENDED
  };

  scoped_ptr<PrefStore> pref_stores_[PREF_STORE_TYPE_MAX + 1];

  bool PrefValueInStore(const wchar_t* name, PrefStoreType type);

  // Returns the pref store type identifying the source that controls the
  // Preference identified by |name|. If none of the sources has a value,
  // INVALID is returned.
  PrefStoreType ControllingPrefStoreForPref(const wchar_t* name);

  // Called during policy refresh after ReadPrefs completes on the thread
  // that initiated the policy refresh. RefreshPolicyPrefsCompletion takes
  // ownership of the |callback| object.
  void RefreshPolicyPrefsCompletion(
      PrefStore* new_managed_pref_store,
      PrefStore* new_recommended_pref_store,
      AfterRefreshCallback* callback);

  // Called during policy refresh to do the ReadPrefs on the FILE thread.
  // RefreshPolicyPrefsOnFileThread takes ownership of the |callback| object.
  void RefreshPolicyPrefsOnFileThread(
      ChromeThread::ID calling_thread_id,
      PrefStore* new_managed_pref_store,
      PrefStore* new_recommended_pref_store,
      AfterRefreshCallback* callback);

  DISALLOW_COPY_AND_ASSIGN(PrefValueStore);
};

#endif  // CHROME_BROWSER_PREF_VALUE_STORE_H_
