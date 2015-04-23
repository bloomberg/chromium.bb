// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prefs/pref_observer_bridge.h"

#include "base/bind.h"
#include "base/prefs/pref_change_registrar.h"

PrefObserverBridge::PrefObserverBridge(id<PrefObserverDelegate> delegate)
    : delegate_(delegate) {
}

PrefObserverBridge::~PrefObserverBridge() {
}

void PrefObserverBridge::ObserveChangesForPreference(
    const std::string& pref_name,
    PrefChangeRegistrar* registrar) {
  PrefChangeRegistrar::NamedChangeCallback callback = base::Bind(
      &PrefObserverBridge::OnPreferenceChanged, base::Unretained(this));
  registrar->Add(pref_name.c_str(), callback);
}

void PrefObserverBridge::OnPreferenceChanged(const std::string& pref_name) {
  [delegate_ onPreferenceChanged:pref_name];
}
