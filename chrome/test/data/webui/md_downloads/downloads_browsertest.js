// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the Material Design downloads page. */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * @constructor
 * @extends {PolymerTest}
*/
function DownloadsBrowserTest() {}

DownloadsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://downloads/manager.html',

  /** @override */
  commandLineSwitches: [{switchName: 'enable-md-downloads'}],

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'layout_tests.js',
  ]),
};

TEST_F('DownloadsBrowserTest', 'layoutTests', function() {
  downloads.layout_tests.registerTests();
  mocha.run();
});
