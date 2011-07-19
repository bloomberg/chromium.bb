// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_IN_PROCESS_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_IN_PROCESS_BROWSER_TEST_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/cros_mock.h"
#include "chrome/test/in_process_browser_test.h"

namespace chromeos {

// Base class for Chromium OS tests wanting to bring up a browser in the
// unit test process and mock some parts of CrosLibrary. Once you mock part of
// CrosLibrary it will be considered as successfully loaded and libraries
// that compose CrosLibrary will be created. Use CrosMock to specify minimum
// set of mocks for you test to succeed.
// See comments for InProcessBrowserTest base class too.
class CrosInProcessBrowserTest : public InProcessBrowserTest {
 public:
  CrosInProcessBrowserTest();
  virtual ~CrosInProcessBrowserTest();

 protected:
  scoped_ptr<CrosMock> cros_mock_;

  // Overriden for things you would normally override TearDown for.
  virtual void TearDownInProcessBrowserTestFixture();

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosInProcessBrowserTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CROS_IN_PROCESS_BROWSER_TEST_H_
