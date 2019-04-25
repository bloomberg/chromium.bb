// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the WebUI resources tests. */

/** @const {string} Path to source root. */
const ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for Polymer Settings elements.
 * @constructor
 * @extends {PolymerTest}
 */
function WebUIResourcesBrowserTest() {}

WebUIResourcesBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overridden by subclasses';
  },

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
  },
};

function WebUIResourcesListPropertyUpdateBehaviorTest() {}

WebUIResourcesListPropertyUpdateBehaviorTest.prototype = {
  __proto__: WebUIResourcesBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/html/list_property_update_behavior.html',

  /** @override */
  extraLibraries: WebUIResourcesBrowserTest.prototype.extraLibraries.concat([
    'list_property_update_behavior_tests.js',
  ]),
};

TEST_F('WebUIResourcesListPropertyUpdateBehaviorTest', 'All', function() {
  mocha.run();
});
