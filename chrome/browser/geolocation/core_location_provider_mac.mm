// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/core_location_provider_mac.h"
#include "chrome/browser/geolocation/core_location_data_provider_mac.h"

#include "base/logging.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

CoreLocationProviderMac::CoreLocationProviderMac() {
  data_provider_ = new CoreLocationDataProviderMac();
  data_provider_->AddRef();
}

CoreLocationProviderMac::~CoreLocationProviderMac() {
  data_provider_->StopUpdating();
  data_provider_->Release();
}

bool CoreLocationProviderMac::StartProvider(bool high_accuracy) {
  data_provider_->StartUpdating(this);
  return true;
}

void CoreLocationProviderMac::StopProvider() {
  data_provider_->StopUpdating();
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
  if(CommandLine::ForCurrentProcess()
     ->HasSwitch(switches::kExperimentalLocationFeatures)) {
    return new CoreLocationProviderMac;
  }
  return NULL;
}
