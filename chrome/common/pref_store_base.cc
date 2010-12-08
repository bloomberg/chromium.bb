// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_store_base.h"

void PrefStoreBase::AddObserver(PrefStore::ObserverInterface* observer) {
  observers_.AddObserver(observer);
}

void PrefStoreBase::RemoveObserver(PrefStore::ObserverInterface* observer) {
  observers_.RemoveObserver(observer);
}

void PrefStoreBase::NotifyPrefValueChanged(const std::string& key) {
  FOR_EACH_OBSERVER(ObserverInterface, observers_, OnPrefValueChanged(key));
}

void PrefStoreBase::NotifyInitializationCompleted() {
  FOR_EACH_OBSERVER(ObserverInterface, observers_, OnInitializationCompleted());
}
