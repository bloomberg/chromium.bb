// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer elements. */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for shared Polymer elements.
 * @constructor
 * @extends {PolymerTest}
*/
function CrElementsBrowserTest() {}

CrElementsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'cr_slider_tests.js',
    'cr_toolbar_search_field_tests.js',
  ]),

  /**
   * Hack: load a page underneath chrome://resources so script errors come
   * from the same "domain" and can be viewed. HTML imports aren't deduped with
   * the current page, but it should be safe to load assert.html twice.
   * @override
   */
  browsePreload: 'chrome://resources/html/assert.html',

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  },
};

TEST_F('CrElementsBrowserTest', 'CrToolbarSearchFieldTest', function() {
  cr_toolbar_search_field.registerTests();
  mocha.run();
});

TEST_F('CrElementsBrowserTest', 'CrSliderTest', function() {
  cr_slider.registerTests();
  mocha.run();
});
