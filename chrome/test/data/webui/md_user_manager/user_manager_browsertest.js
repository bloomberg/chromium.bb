// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the Material Design user manager page. */

/** @const {string} Path to root from chrome/test/data/webui/md_user_manager/ */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * @constructor
 * @extends {PolymerTest}
*/
function UserManagerBrowserTest() {}

UserManagerBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-user-manager/',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    '../settings/test_browser_proxy.js',
    'control_bar_tests.js',
    'create_profile_tests.js',
    'import_supervised_user_tests.js',
    'test_profile_browser_proxy.js',
    'user_manager_pages_tests.js',
  ]),

  /** @override */
  runAccessibilityChecks: false,
};

TEST_F('UserManagerBrowserTest', 'UserManagerTest', function() {
  // Disable 'pod-row' so it won't handle click events after we clear the body.
  $('pod-row').disabled = true;

  user_manager.control_bar_tests.registerTests();
  user_manager.create_profile_tests.registerTests();
  user_manager.import_supervised_user_tests.registerTests();
  user_manager.user_manager_pages_tests.registerTests();
  mocha.run();
});
