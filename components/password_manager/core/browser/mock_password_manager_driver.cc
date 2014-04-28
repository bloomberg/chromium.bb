// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/mock_password_manager_driver.h"

namespace password_manager {

// Out-of-lining these methods only because the chromium-style clang plugin
// complains that complex constructors and destructors should not be inlined.
//
// http://www.chromium.org/developers/coding-style/chromium-style-checker-errors
MockPasswordManagerDriver::MockPasswordManagerDriver() {
}

MockPasswordManagerDriver::~MockPasswordManagerDriver() {
}

}  // namespace password_manager
