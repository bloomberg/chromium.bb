// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_STORE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_STORE_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/common/pref_store.h"

class DictionaryValue;
class PrefService;
class Value;

// This PrefStore keeps track of preferences set by extensions: for example,
// proxy settings. A stack of relevant extension IDs is stored in order of
// their addition to this PrefStore. For each preference, the last-added
// enabled extension that tries to set it overrules any others.
class ExtensionPrefStore : public PrefStore {
 public:
  explicit ExtensionPrefStore(PrefService* pref_service);
  virtual ~ExtensionPrefStore();

  // The PrefService creates the ExtensionPrefStore, so we need to be able to
  // defer setting the PrefService here until after construction.
  void SetPrefService(PrefService* pref_service) {
    pref_service_ = pref_service;
  }

  // Begins tracking the preference and value an extension wishes to set. This
  // must be called each time an extension API tries to set a preference.
  virtual void InstallExtensionPref(std::string extension_id,
                                    const wchar_t* pref_path,
                                    Value* pref_value);

  // Removes an extension and all its preference settings from this PrefStore.
  // This must be called when an extension is uninstalled or disabled.
  virtual void UninstallExtension(std::string extension_id);

  // PrefStore methods:
  virtual DictionaryValue* prefs() { return prefs_.get(); }

  virtual PrefReadError ReadPrefs() { return PREF_READ_ERROR_NONE; }

 protected:
  // Returns a vector of the extension IDs in the extension_stack_.
  // This should only be accessed by subclasses for unit-testing.
  void GetExtensionIDs(std::vector<std::string>* result);

 private:
  // The pref service referring to this pref store. Weak reference.
  PrefService* pref_service_;

  // Maps preference paths to their values.
  typedef std::map<const wchar_t*, Value*> PrefValueMap;

  // Applies the highest-priority extension's setting for the given preference
  // path, or clears the setting in this PrefStore if no extensions wish to
  // control it.
  void UpdateOnePref(const wchar_t* path);

  // Updates each preference in the |pref_values| list.
  void UpdatePrefs(const PrefValueMap* pref_values);

  // A cache of the highest-priority values for each preference that any
  // extension is controlling, for quick read access. Owns the stored values.
  scoped_ptr<DictionaryValue> prefs_;

  // Associates an extension ID with the prefs it sets.
  struct ExtensionPrefs {
    std::string extension_id;
    PrefValueMap* pref_values;
  };

  // A pseudo-stack of extensions and their preferences. Extensions are always
  // added to the head, but may be removed from the middle. This stack owns
  // the values in the extensions' PrefValueMaps.
  typedef std::list<ExtensionPrefs*> ExtensionStack;
  ExtensionStack extension_stack_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefStore);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_STORE_H_
