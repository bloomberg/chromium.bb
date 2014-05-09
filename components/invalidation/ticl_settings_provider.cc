// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/ticl_settings_provider.h"

namespace invalidation {

TiclSettingsProvider::Observer::~Observer() {
}

TiclSettingsProvider::TiclSettingsProvider() {
}

TiclSettingsProvider::~TiclSettingsProvider() {
}

void TiclSettingsProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TiclSettingsProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TiclSettingsProvider::FireOnUseGCMChannelChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, OnUseGCMChannelChanged());
}

}  // namespace invalidation
