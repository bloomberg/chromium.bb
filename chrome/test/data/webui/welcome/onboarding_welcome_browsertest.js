// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer welcome tests on onboarding-welcome UI. */

/** @const {string} Path to source root. */
const ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/ui/webui/welcome/nux_helper.h"');

/**
 * Test fixture for Polymer onboarding welcome elements.
 */
const OnboardingWelcomeBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overridden by subclasses';
  }

  get extraLibraries() {
    return PolymerTest.getLibraries(ROOT_PATH).concat([
      '../test_browser_proxy.js',
    ]);
  }

  /** @override */
  get featureList() {
    return ['nux::kNuxOnboardingForceEnabled', ''];
  }
};

OnboardingWelcomeEmailChooserTest = class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/email/email_chooser.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'email_chooser_test.js',
      'test_nux_email_proxy.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeEmailChooserTest', 'All', function() {
  mocha.run();
});
