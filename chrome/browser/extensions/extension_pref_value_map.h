// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_VALUE_MAP_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_VALUE_MAP_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/time.h"
#include "chrome/browser/prefs/value_map_pref_store.h"

// Non-persistent data container that is shared by ExtensionPrefStores. All
// extension pref values (incognito and regular) are stored herein and
// provided to ExtensionPrefStores.
//
// The semantics of the ExtensionPrefValueMap are:
// - The precedence of extensions is determined by their installation time.
//   The extension that has been installed later takes higher precedence.
// - If two extensions set a value for the same preference, the following
//   rules determine which value becomes effective (visible).
// - The effective regular extension pref value is determined by the regular
//   extension pref value of the extension with the highest precedence.
// - The effective incognito extension pref value is determined by the incognito
//   extension pref value of the extension with the highest precedence, unless
//   another extension with higher precedence overrides it with a regular
//   extension pref value.
//
// The following table illustrates the behavior:
//   A.reg | A.inc | B.reg | B.inc | E.reg | E.inc
//     1   |   -   |   -   |   -   |   1   |   1
//     1   |   2   |   -   |   -   |   1   |   2
//     1   |   -   |   3   |   -   |   3   |   3
//     1   |   -   |   -   |   4   |   1   |   4
//     1   |   2   |   3   |   -   |   3   |   3(!)
//     1   |   2   |   -   |   4   |   1   |   4
//     1   |   2   |   3   |   4   |   3   |   4
// A = extension A, B = extension B, E = effective value
// .reg = regular value
// .inc = incognito value
// Extension B has higher precedence than A.
class ExtensionPrefValueMap {
 public:
  // Observer interface for monitoring ExtensionPrefValueMap.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the value for the given |key| set by one of the extensions
    // changes. This does not necessarily mean that the effective value has
    // changed.
    virtual void OnPrefValueChanged(const std::string& key) = 0;
    // Notification about the ExtensionPrefValueMap being fully initialized.
    virtual void OnInitializationCompleted() = 0;
    // Called when the ExtensionPrefValueMap is being destroyed. When called,
    // observers must unsubscribe.
    virtual void OnExtensionPrefValueMapDestruction() = 0;
  };

  ExtensionPrefValueMap();
  virtual ~ExtensionPrefValueMap();

  // Set an extension preference |value| for |key| of extension |ext_id|.
  // Takes ownership of |value|.
  // Note that regular extension pref values need to be reported to
  // incognito and to regular ExtensionPrefStores.
  // Precondition: the extension must be registered.
  void SetExtensionPref(const std::string& ext_id,
                        const std::string& key,
                        bool incognito,
                        Value* value);

  // Remove the extension preference value for |key| of extension |ext_id|.
  // Precondition: the extension must be registered.
  void RemoveExtensionPref(const std::string& ext_id,
                           const std::string& key,
                           bool incognito);

  // Tell the store it's now fully initialized.
  void NotifyInitializationCompleted();

  // Registers the time when an extension |ext_id| is installed.
  void RegisterExtension(const std::string& ext_id,
                         const base::Time& install_time,
                         bool is_enabled);

  // Deletes all entries related to extension |ext_id|.
  void UnregisterExtension(const std::string& ext_id);

  // Hides or makes the extension preference values of the specified extension
  // visible.
  void SetExtensionState(const std::string& ext_id, bool is_enabled);

  // Adds an observer and notifies it about the currently stored keys.
  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  const Value* GetEffectivePrefValue(const std::string& key,
                                     bool incognito) const;

 private:
  struct ExtensionEntry {
    // Installation time of the extension.
    base::Time install_time;
    // Whether extension is enabled in the profile.
    bool enabled;
    // Regular preferences.
    PrefValueMap reg_preferences;
    // Incognito preferences, empty for regular ExtensionPrefStore.
    PrefValueMap inc_preferences;
  };

  typedef std::map<std::string, ExtensionEntry*> ExtensionEntryMap;

  const PrefValueMap* GetExtensionPrefValueMap(const std::string& ext_id,
                                               bool incognito) const;

  PrefValueMap* GetExtensionPrefValueMap(const std::string& ext_id,
                                         bool incognito);

  // Returns all keys of pref values that are set by the extension of |entry|,
  // regardless whether they are set for incognito or regular pref values.
  void GetExtensionControlledKeys(const ExtensionEntry& entry,
                                  std::set<std::string>* out) const;

  void NotifyOfDestruction();
  void NotifyPrefValueChanged(const std::string& key);
  void NotifyPrefValueChanged(const std::set<std::string>& keys);

  // Mapping of which extension set which preference value. The effective
  // preferences values (i.e. the ones with the highest precedence)
  // are stored in ExtensionPrefStores.
  ExtensionEntryMap entries_;

  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefValueMap);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_VALUE_MAP_H_
