// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/test/in_process_browser_test.h"

namespace chromeos {

CrosInProcessBrowserTest::CrosInProcessBrowserTest() {
  cros_mock_.reset(new CrosMock());
}

CrosInProcessBrowserTest::~CrosInProcessBrowserTest() {
}

void CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture() {
  cros_mock_->TearDownMocks();
}

}  // namespace chromeos
