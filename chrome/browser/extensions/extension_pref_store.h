// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_STORE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_STORE_H_
#pragma once

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/pref_store.h"

class DictionaryValue;
class Extension;
class PrefService;
class Profile;
class Value;

// This PrefStore keeps track of preferences set by extensions: for example,
// proxy settings. A stack of relevant extensions is stored in order of
// their addition to this PrefStore. For each preference, the last-added
// enabled extension that tries to set it overrules any others.
class ExtensionPrefStore : public PrefStore,
                           public NotificationObserver {
 public:
  ExtensionPrefStore(Profile* profile, PrefNotifier::PrefStoreType type);
  virtual ~ExtensionPrefStore();

  // Begins tracking the preference and value an extension wishes to set. This
  // must be called each time an extension API tries to set a preference.
  // The ExtensionPrefStore will take ownership of the |pref_value|.
  virtual void InstallExtensionPref(Extension* extension,
                                    const char* pref_path,
                                    Value* pref_value);

  // Removes an extension and all its preference settings from this PrefStore.
  // This must be called when an extension is uninstalled or disabled.
  virtual void UninstallExtension(Extension* extension);

  // PrefStore methods:
  virtual DictionaryValue* prefs() { return prefs_.get(); }

  virtual PrefReadError ReadPrefs() { return PREF_READ_ERROR_NONE; }

  // The type passed as Details for an EXTENSION_PREF_CHANGED notification.
  // The nested pairs are <extension, <pref_path, pref_value> >. This is here,
  // rather than in (say) notification_type.h, to keep the dependency on
  // std::pair out of the many places that include notification_type.h.
  typedef std::pair<Extension*, std::pair<const char*, Value*> >
      ExtensionPrefDetails;

 protected:
  // Returns a vector of the extension IDs in the extension_stack_.
  // This should only be accessed by subclasses for unit-testing.
  void GetExtensionIDs(std::vector<std::string>* result);

  // Returns the applicable pref service from the profile (if we have one) or
  // the browser's local state. This should only be accessed or overridden by
  // subclasses for unit-testing.
  virtual PrefService* GetPrefService();

 private:
  // Maps preference paths to their values.
  typedef std::map<const char*, Value*> PrefValueMap;

  // Applies the highest-priority extension's setting for the given preference
  // path to the |prefs_| store, or clears the setting there if no extensions
  // wish to control it.
  void UpdateOnePref(const char* path);

  // Updates each preference in the key set of the |pref_values| map.
  void UpdatePrefs(const PrefValueMap* pref_values);

  // Registers this as an observer for relevant notifications.
  void RegisterObservers();

  // Responds to observed notifications.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // A cache of the highest-priority values for each preference that any
  // extension is controlling, for quick read access. Owns the stored values.
  scoped_ptr<DictionaryValue> prefs_;

  // Associates an extension with the prefs it sets. Owns the pref values.
  struct ExtensionPrefs {
    ExtensionPrefs(Extension* extension, PrefValueMap* values);
    ~ExtensionPrefs();

    Extension* extension;
    PrefValueMap* pref_values;
  };

  // A pseudo-stack of extensions and their preferences. Extensions are always
  // added to the head, but may be removed from the middle.
  typedef std::list<ExtensionPrefs*> ExtensionStack;
  ExtensionStack extension_stack_;

  NotificationRegistrar notification_registrar_;

  // Weak reference to the profile whose extensions we're interested in. May be
  // NULL (for the local-state preferences), in which case we watch all
  // extensions.
  Profile* profile_;

  // My PrefStore type, assigned by the PrefValueStore.
  PrefNotifier::PrefStoreType type_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefStore);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_STORE_H_
