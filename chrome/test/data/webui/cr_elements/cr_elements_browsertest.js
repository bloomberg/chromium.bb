// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Chromium Polymer elements tests. */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for Chromium Polymer elements.
 * @constructor
 * @extends {PolymerTest}
*/
function CrElementsBrowserTest() {}

CrElementsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  // List tests for individual elements.
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'cr_checkbox_tests.js',
  ]),
};

// Runs all tests.
TEST_F('CrElementsBrowserTest', 'CrElementsTest', function() {
  // Register mocha tests for each element.
  cr_checkbox.registerTests();

  // Run all registered tests.
  mocha.run();
});
