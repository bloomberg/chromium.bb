// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SETTINGS_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_store.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class PersistentPrefStore;
class Profile;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

// TODO(bauerb): This class is not yet fully functional.
// This class syncs managed user settings from a server, which are mapped to
// preferences. The downloaded settings are persisted in a PrefStore (which is
// not directly hooked up to the PrefService; it's just used internally).
// Settings are key-value pairs, where the key uniquely identifies the setting.
// The value is a string containing a JSON serialization of an arbitrary value,
// which is the value of the setting.
//
// There are two kinds of settings handled by this class: Atomic and split
// settings.
// Atomic settings consist of a single key (which will be mapped to a pref key)
// and a single (arbitrary) value.
// Split settings encode a dictionary value and are stored as multiple Sync
// items, one for each dictionary entry. The key for each of these Sync items
// is the key of the split setting, followed by a separator (':') and the key
// for the dictionary entry. The value of the Sync item is the value of the
// dictionary entry.
//
// As an example, a split setting with key "Moose" and value
//   {
//     "foo": "bar",
//     "baz": "blurp"
//   }
// would be encoded as two sync items, one with key "Moose:foo" and value "bar",
// and one with key "Moose:baz" and value "blurp".
class ManagedUserSettingsService : public BrowserContextKeyedService,
                                   public PrefStore::Observer {
 public:
  // A callback whose first parameter is a dictionary containing all managed
  // user settings. If the dictionary is NULL, it means that the service is
  // inactive, i.e. the user is not managed.
  typedef base::Callback<void(const base::DictionaryValue*)> SettingsCallback;

  ManagedUserSettingsService();
  virtual ~ManagedUserSettingsService();

  // Initializes the service by loading its settings from the |pref_store|.
  // Use this method in tests to inject a different PrefStore than the
  // default one.
  void Init(scoped_refptr<PersistentPrefStore> pref_store);

  // Adds a callback to be called when managed user settings are initially
  // available, or when they change.
  void Subscribe(const SettingsCallback& callback);

  // Activates the service. This happens when the user is managed.
  void Activate();

  // Whether managed user settings are available.
  bool IsReady();

  // Clears all managed user settings and items.
  void Clear();

  // Sets the setting with the given |key| to a copy of the given |value|.
  void SetLocalSettingForTesting(const std::string& key,
                                 scoped_ptr<base::Value> value);

  // BrowserContextKeyedService implementation:
  virtual void Shutdown() OVERRIDE;

  // PrefStore::Observer implementation:
  virtual void OnPrefValueChanged(const std::string& key) OVERRIDE;
  virtual void OnInitializationCompleted(bool success) OVERRIDE;

 private:
  base::DictionaryValue* GetOrCreateDictionary(const std::string& key) const;
  base::DictionaryValue* GetAtomicSettings() const;
  base::DictionaryValue* GetSplitSettings() const;
  base::DictionaryValue* GetQueuedItems() const;

  // Returns a dictionary with all managed user settings if the service is
  // active, or NULL otherwise.
  scoped_ptr<base::DictionaryValue> GetSettings();

  // Sends the settings to all subscribers. This method should be called by the
  // subclass whenever the settings change.
  void InformSubscribers();

  // Used for persisting the settings. Unlike other PrefStores, this one is not
  // directly hooked up to the PrefService.
  scoped_refptr<PersistentPrefStore> store_;

  bool active_;

  // A set of local settings that are fixed and not configured remotely.
  scoped_ptr<base::DictionaryValue> local_settings_;

  std::vector<SettingsCallback> subscribers_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserSettingsService);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SETTINGS_SERVICE_H_
