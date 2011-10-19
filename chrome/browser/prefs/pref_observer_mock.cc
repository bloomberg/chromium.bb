// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_observer_mock.h"
#include "chrome/common/chrome_notification_types.h"

PrefObserverMock::PrefObserverMock() {}

PrefObserverMock::~PrefObserverMock() {}

void PrefObserverMock::Expect(const PrefService* prefs,
                              const std::string& pref_name,
                              const Value* value) {
  EXPECT_CALL(*this, Observe(int(chrome::NOTIFICATION_PREF_CHANGED),
                             content::Source<PrefService>(prefs),
                             Property(&content::Details<std::string>::ptr,
                                      Pointee(pref_name))))
      .With(PrefValueMatches(prefs, pref_name, value));
}
