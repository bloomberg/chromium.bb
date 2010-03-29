// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_IN_PROCESS_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_IN_PROCESS_BROWSER_TEST_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/mock_language_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/mock_power_library.h"
#include "chrome/browser/chromeos/cros/mock_synaptics_library.h"
#include "chrome/test/in_process_browser_test.h"
#include "third_party/cros/chromeos_language.h"

namespace chromeos {

// Base class for Chromium OS tests wanting to bring up a browser in the
// unit test process and mock some parts of CrosLibrary. Once you mock part of
// CrosLibrary it will be considered as successfully loaded and libraries
// that compose CrosLibrary will be created. The issue here is that you do
// need to specify minimum set of mocks for you test to succeed.
// CrosInProcessBrowserTest defines minimum set of mocks that is used
// by status area elements (network, input language, power).
// See comments for InProcessBrowserTest base class too.
class CrosInProcessBrowserTest : public InProcessBrowserTest {
 public:
  CrosInProcessBrowserTest();
  virtual ~CrosInProcessBrowserTest();

 protected:
  // This method setups basic mocks that are used by status area items:
  // LibraryLoader, Language, Network, Power, Synaptics libraries.
  // Add call to this method at the beginning of your
  // SetUpInProcessBrowserTestFixture.
  void InitStatusAreaMocks();

  // Initialization of CrosLibrary mock loader. If you intend calling
  // separate init methods for mocks call this one first.
  void InitMockLibraryLoader();

  // Initialization of mocks.
  void InitMockLanguageLibrary();
  void InitMockNetworkLibrary();
  void InitMockPowerLibrary();
  void InitMockSynapticsLibrary();

  // This method setups corresponding expectations for basic mocks that
  // are used by status area items.
  // Make sure that InitStatusAreaMocks was called before.
  // Add call to this method in your SetUpInProcessBrowserTestFixture.
  // They are all configured with RetiresOnSaturation().
  // Once such expectation is used it won't block expectations you've defined.
  void SetStatusAreaMocksExpectations();

  // Methods to setup minimal mocks expectations for status area.
  void SetLanguageLibraryStatusAreaExpectations();
  void SetNetworkLibraryStatusAreaExpectations();
  void SetPowerLibraryStatusAreaExpectations();
  void SetSynapticsLibraryExpectations();

  // Overriden for things you would normally override TearDown for.
  virtual void TearDownInProcessBrowserTestFixture();

  // TestApi gives access to CrosLibrary private members.
  chromeos::CrosLibrary::TestApi* test_api();

  // Mocks, destroyed by CrosLibrary class.
  MockLibraryLoader* loader_;
  MockLanguageLibrary* mock_language_library_;
  MockNetworkLibrary* mock_network_library_;
  MockPowerLibrary* mock_power_library_;
  MockSynapticsLibrary* mock_synaptics_library_;

  ImePropertyList ime_properties_;
  InputLanguage language_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosInProcessBrowserTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CROS_IN_PROCESS_BROWSER_TEST_H_
