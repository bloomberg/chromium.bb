// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREFERENCES_MANAGER_H_
#define CHROME_BROWSER_PREFS_PREFERENCES_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"

namespace test {
class PreferencesManagerTest;
}

class PrefChangeRegistrar;
class PrefService;
class Profile;

// Implementation of prefs::mojom::PreferencesManager that accepts a single
// prefs::mojom::PreferencesObserver.
//
// After calling AddObserver PreferencesManager will begin observing changes to
// the requested preferences, notifying the client of all changes.
class PreferencesManager : public prefs::mojom::PreferencesManager {
 public:
  PreferencesManager(prefs::mojom::PreferencesObserverPtr client,
                     Profile* profile);
  ~PreferencesManager() override;

 private:
  friend class test::PreferencesManagerTest;

  // PrefChangeRegistrar::NamedChangeCallback:
  void PreferenceChanged(const std::string& preference_name);

  // mojom::PreferencesManager:
  void SetPreferences(
      std::unique_ptr<base::DictionaryValue> preferences) override;
  void Subscribe(const std::vector<std::string>& preferences) override;

  // Tracks the desired preferences, and listens for updates.
  std::unique_ptr<PrefChangeRegistrar> preferences_change_registrar_;
  prefs::mojom::PreferencesObserverPtr client_;
  PrefService* service_;

  // Used to prevent notifying |client_| of changes caused by it calling
  // SetPreferences.
  bool setting_preferences_;

  DISALLOW_COPY_AND_ASSIGN(PreferencesManager);
};

#endif  // CHROME_BROWSER_PREFS_PREFERENCES_MANAGER_H_
