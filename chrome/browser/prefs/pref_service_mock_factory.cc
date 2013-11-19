// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_mock_factory.h"

#include "base/prefs/testing_pref_store.h"

PrefServiceMockFactory::PrefServiceMockFactory() {
    user_prefs_ = new TestingPrefStore;
}

PrefServiceMockFactory::~PrefServiceMockFactory() {}
