// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/mock_pref_change_callback.h"

#include "base/bind.h"
#include "chrome/common/chrome_notification_types.h"

MockPrefChangeCallback::MockPrefChangeCallback(PrefServiceBase* prefs)
    : prefs_(prefs) {
}

MockPrefChangeCallback::~MockPrefChangeCallback() {}

PrefChangeRegistrar::NamedChangeCallback MockPrefChangeCallback::GetCallback() {
  return base::Bind(&MockPrefChangeCallback::OnPreferenceChanged,
                    base::Unretained(this));
}

void MockPrefChangeCallback::Expect(const std::string& pref_name,
                                    const Value* value) {
  EXPECT_CALL(*this, OnPreferenceChanged(pref_name))
      .With(PrefValueMatches(prefs_, pref_name, value));
}
