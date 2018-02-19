// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Sign-in web UI tests. */

/** @const {string} Path to source root. */
const ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "base/command_line.h"');
GEN('#include "chrome/test/data/webui/signin_browsertest.h"');

/**
 * Test fixture for
 * chrome/browser/resources/signin/dice_sync_confirmation/sync_confirmation.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
var SigninSyncConfirmationTest = class extends PolymerTest {
  /** @override */
  get typedefCppFixture() {
    return 'SigninBrowserTest';
  }

  /** @override */
  testGenPreamble() {
    GEN('  EnableUnity();');
  }

  /** @override */
  get browsePreload() {
    return 'chrome://sync-confirmation/sync_confirmation_app.html';
  }

  /** @override */
  get extraLibraries() {
    return PolymerTest.getLibraries(ROOT_PATH).concat([
      'sync_confirmation_test.js',
    ]);
  }
};

TEST_F('SigninSyncConfirmationTest', 'DialogWithDice', function() {
  mocha.run();
});
