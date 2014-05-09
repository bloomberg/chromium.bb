// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_TICL_PROFILE_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_INVALIDATION_TICL_PROFILE_SETTINGS_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/prefs/pref_change_registrar.h"
#include "components/invalidation/ticl_settings_provider.h"

class Profile;

namespace invalidation {

// A specialization of TiclSettingsProvider that reads settings from user prefs.
class TiclProfileSettingsProvider : public TiclSettingsProvider {
 public:
  explicit TiclProfileSettingsProvider(Profile* profile);
  virtual ~TiclProfileSettingsProvider();

  // TiclInvalidationServiceSettingsProvider:
  virtual bool UseGCMChannel() const OVERRIDE;

 private:
  PrefChangeRegistrar registrar_;
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(TiclProfileSettingsProvider);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_TICL_PROFILE_SETTINGS_PROVIDER_H_
