// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/mock_login_display_host.h"

#include "chrome/test/base/ui_test_utils.h"

namespace chromeos {

MockLoginDisplayHost::MockLoginDisplayHost() {
}

MockLoginDisplayHost::~MockLoginDisplayHost() {
}

void MockLoginDisplayHost::StartWizard(
    const std::string& name,
    scoped_ptr<base::DictionaryValue> value) {
  return StartWizardPtr(name, value.get());
}

}  // namespace chromeos
