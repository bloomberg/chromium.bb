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
function DownloadsItemTest() {}

DownloadsItemTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://downloads/item.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'item_tests.js',
  ]),
};

TEST_F('DownloadsItemTest', 'All', function() {
  mocha.run();
});


/**
 * @constructor
 * @extends {PolymerTest}
 */
function DownloadsLayoutTest() {}

DownloadsLayoutTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://downloads/manager.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'layout_tests.js',
  ]),
};

TEST_F('DownloadsLayoutTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {PolymerTest}
 */
function DownloadsToolbarTest() {}

DownloadsToolbarTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://downloads/toolbar.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'toolbar_tests.js',
  ]),
};

TEST_F('DownloadsToolbarTest', 'All', function() {
  mocha.run();
});
