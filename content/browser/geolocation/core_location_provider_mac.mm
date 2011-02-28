// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/core_location_provider_mac.h"

#include "base/logging.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/geolocation/core_location_data_provider_mac.h"

CoreLocationProviderMac::CoreLocationProviderMac()
    : is_updating_(false) {
  data_provider_ = new CoreLocationDataProviderMac();
  data_provider_->AddRef();
}

CoreLocationProviderMac::~CoreLocationProviderMac() {
  data_provider_->StopUpdating();
  data_provider_->Release();
}

bool CoreLocationProviderMac::StartProvider(bool high_accuracy) {
  // StartProvider maybe called multiple times. For example, to update the high
  // accuracy hint.
  // TODO(jknotten): Support high_accuracy hint in underlying data provider.
  if (is_updating_)
    return true;

  is_updating_ = data_provider_->StartUpdating(this);
  return true;
}

void CoreLocationProviderMac::StopProvider() {
  data_provider_->StopUpdating();
  is_updating_ = false;
}

void CoreLocationProviderMac::GetPosition(Geoposition* position) {
  DCHECK(position);
  *position = position_;
  DCHECK(position->IsInitialized());
}

void CoreLocationProviderMac::SetPosition(Geoposition* position) {
  DCHECK(position);
  position_ = *position;
  DCHECK(position->IsInitialized());

  UpdateListeners();
}

LocationProviderBase* NewSystemLocationProvider() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExperimentalLocationFeatures)) {
    return new CoreLocationProviderMac;
  }
  return NULL;
}
