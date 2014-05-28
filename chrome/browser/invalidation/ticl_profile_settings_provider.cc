// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/ticl_profile_settings_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

namespace invalidation {

TiclProfileSettingsProvider::TiclProfileSettingsProvider(Profile* profile)
    : profile_(profile) {
  registrar_.Init(profile->GetPrefs());
  registrar_.Add(
      prefs::kInvalidationServiceUseGCMChannel,
      base::Bind(&TiclProfileSettingsProvider::FireOnUseGCMChannelChanged,
                 base::Unretained(this)));
  registrar_.Add(
      prefs::kGCMChannelEnabled,
      base::Bind(&TiclProfileSettingsProvider::FireOnUseGCMChannelChanged,
                 base::Unretained(this)));
}

TiclProfileSettingsProvider::~TiclProfileSettingsProvider() {
}

bool TiclProfileSettingsProvider::UseGCMChannel() const {
  if (!gcm::GCMProfileService::IsGCMEnabled(profile_)) {
    // Do not try to use GCM channel if GCM is disabled.
    return false;
  }

  if (profile_->GetPrefs()->GetBoolean(
          prefs::kInvalidationServiceUseGCMChannel)) {
    // Use GCM channel if it was enabled via prefs.
    return true;
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInvalidationUseGCMChannel)) {
    // Use GCM channel if it was enabled via a command-line switch.
    return true;
  }

  // By default, do not use GCM channel.
  return false;
}

}  // namespace invalidation
