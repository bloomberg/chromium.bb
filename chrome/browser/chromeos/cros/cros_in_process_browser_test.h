// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/in_process_browser_test.h"

#include "chrome/browser/chromeos/cros/mock_language_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/mock_power_library.h"
#include "chrome/browser/chromeos/cros/mock_synaptics_library.h"
#include "third_party/cros/chromeos_language.h"

namespace chromeos {

// Base class for Chromium OS tests wanting to bring up a browser in the
// unit test process and mock some parts of CrosLibrary. Once you mock part of
// CrosLibrary it will be considered as successfully loaded and libraries
// that compose CrosLibrary will be created. The issue here is that you do
// need to specify minimum set of mocks for you test to succeed.
// CrosInProcessBrowserTest creates minimum set of mocks that is used
// by status bar elements (network, input language, power).
// See comments for InProcessBrowserTest base class too.
class CrosInProcessBrowserTest : public InProcessBrowserTest {
 public:
  CrosInProcessBrowserTest();
  virtual ~CrosInProcessBrowserTest();

 protected:
  // These functions are overriden from InProcessBrowserTest.
  // Called before your individual test fixture method is run, but after most
  // of the overhead initialization has occured.
  // This method set ups basic mocks that are used by status bar items and
  // setups corresponding expectations for method calls.
  // If your test needs to initialize other mocks override this method
  // and add call to base version. To change method calls expectations just
  // provide new macros. Most recent expectation will be used.
  virtual void SetUpInProcessBrowserTestFixture();

  // Overriden for things you would normally override TearDown for.
  virtual void TearDownInProcessBrowserTestFixture();

  // Mocks, destroyed by CrosLibrary class.
  MockLanguageLibrary* mock_language_library_;
  MockLibraryLoader* loader_;
  MockNetworkLibrary* mock_network_library_;
  MockPowerLibrary* mock_power_library_;
  MockSynapticsLibrary* mock_synaptics_library_;

  ImePropertyList ime_properties_;
  InputLanguage language_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosInProcessBrowserTest);
};

}  // namespace chromeos
