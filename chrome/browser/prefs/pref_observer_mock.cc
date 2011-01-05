// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_observer_mock.h"

PrefObserverMock::PrefObserverMock() {}

PrefObserverMock::~PrefObserverMock() {}

void PrefObserverMock::Expect(const PrefService* prefs,
                              const std::string& pref_name,
                              const Value* value) {
  EXPECT_CALL(*this, Observe(NotificationType(NotificationType::PREF_CHANGED),
                             Source<PrefService>(prefs),
                             Property(&Details<std::string>::ptr,
                                      Pointee(pref_name))))
      .With(PrefValueMatches(prefs, pref_name, value));
}
