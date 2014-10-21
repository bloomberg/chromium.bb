// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_PREF_STORE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_PREF_STORE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_store.h"
#include "chrome/browser/supervised_user/supervised_users.h"

namespace base {
class DictionaryValue;
class Value;
}

class PrefValueMap;
class SupervisedUserSettingsService;

// A PrefStore that gets its values from supervised user settings via the
// SupervisedUserSettingsService passed in at construction.
class SupervisedUserPrefStore : public PrefStore {
 public:
  SupervisedUserPrefStore(
      SupervisedUserSettingsService* supervised_user_settings_service);

  // PrefStore overrides:
  bool GetValue(const std::string& key,
                const base::Value** value) const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;

 private:
  ~SupervisedUserPrefStore() override;

  void OnNewSettingsAvailable(const base::DictionaryValue* settings);

  scoped_ptr<PrefValueMap> prefs_;

  ObserverList<PrefStore::Observer, true> observers_;

  base::WeakPtrFactory<SupervisedUserPrefStore> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_PREF_STORE_H_
