// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_IN_PROCESS_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_IN_PROCESS_BROWSER_TEST_H_

#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace chromeos {

class MockNetworkLibrary;

// Base class for Chromium OS tests wanting to bring up a browser in the
// browser test process and provide MockNetworkLibrary with preset behaviors.
class CrosInProcessBrowserTest : public InProcessBrowserTest {
 public:
  CrosInProcessBrowserTest();
  virtual ~CrosInProcessBrowserTest();

  // InProcessBrowserTest overrides:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE;

 private:
  // Sets up basic mocks that are used by status area items.
  void InitStatusAreaMocks();

  // Sets up corresponding expectations for basic mocks that
  // are used by status area items.
  // Make sure that InitStatusAreaMocks was called before.
  // They are all configured with RetiresOnSaturation().
  // Once such expectation is used it won't block expectations you've defined.
  void SetStatusAreaMocksExpectations();

  // Destroyed by NetworkLibrary::Shutdown().
  MockNetworkLibrary* mock_network_library_;

  // Stuff used for mock_network_library_.
  WifiNetworkVector wifi_networks_;
  WimaxNetworkVector wimax_networks_;
  CellularNetworkVector cellular_networks_;
  VirtualNetworkVector virtual_networks_;
  std::string empty_string_;

  DISALLOW_COPY_AND_ASSIGN(CrosInProcessBrowserTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CROS_IN_PROCESS_BROWSER_TEST_H_
