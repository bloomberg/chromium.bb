// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_TICL_PROFILE_SETTINGS_PROVIDER_H_
#define COMPONENTS_INVALIDATION_IMPL_TICL_PROFILE_SETTINGS_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/prefs/pref_change_registrar.h"
#include "components/invalidation/impl/ticl_settings_provider.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace invalidation {

// A specialization of TiclSettingsProvider that reads settings from user prefs.
class TiclProfileSettingsProvider : public TiclSettingsProvider {
 public:
  explicit TiclProfileSettingsProvider(PrefService* prefs);
  ~TiclProfileSettingsProvider() override;

  // Register prefs to be used by per-Profile instances of this class which
  // store invalidation state in Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // TiclInvalidationServiceSettingsProvider:
  bool UseGCMChannel() const override;

 private:
  PrefChangeRegistrar registrar_;
  PrefService* const prefs_;

  DISALLOW_COPY_AND_ASSIGN(TiclProfileSettingsProvider);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_TICL_PROFILE_SETTINGS_PROVIDER_H_
