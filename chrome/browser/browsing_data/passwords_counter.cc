// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/passwords_counter.h"
#include "chrome/common/pref_names.h"

PasswordsCounter::PasswordsCounter()
    : pref_name_(prefs::kDeletePasswords) {
}

PasswordsCounter::~PasswordsCounter() {
}

const std::string& PasswordsCounter::GetPrefName() const {
  return pref_name_;
}

void PasswordsCounter::Count() {
  // TODO(msramek): Count the passwords of |profile|
  // and call |ReportResult| when finished.
}
