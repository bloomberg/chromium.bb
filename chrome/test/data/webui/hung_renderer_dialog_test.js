// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for generated tests.
 * @extends {testing.Test}
 */
function HungRendererDialogUITest() {};

HungRendererDialogUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Define the C++ fixture class and include it.
   * @type {?string}
   * @override
   */
  typedefCppFixture: 'HungRendererDialogUITest',

  /**
   * Show the hung renderer dialog.
   */
  testGenPreamble: function() {
    GEN('ShowHungRendererDialogInternal();');
  },
};

// Include the bulk of c++ code.
GEN('#include "chrome/test/data/webui/hung_renderer_dialog_ui_test-inl.h"');

// Constructors and destructors must be provided in .cc to prevent clang errors.
GEN('HungRendererDialogUITest::HungRendererDialogUITest() {}');
GEN('HungRendererDialogUITest::~HungRendererDialogUITest() {}');

/**
 * Confirms that the URL is correct.
 */
TEST_F('HungRendererDialogUITest', 'testURL', function() {
  expectEquals(chrome.expectedUrl, window.location.href);
});

/**
 * Confirms that the theme graphic is loaded and is of a reasonable size. This
 * validates that we are integrating properly with the theme source.
 */
TEST_F('HungRendererDialogUITest', 'testThemeGraphicIntegration', function() {
  var themeGraphic = $('theme-graphic');
  assertNotEquals(null, themeGraphic);
  expectGT(themeGraphic.width, 0);
  expectGT(themeGraphic.height, 0);
});

/**
 * Confirms that the DOM was updated such that a list item representing the
 * frozen tab was added to the list of frozen tabs.  Also confirms that the list
 * item text content is the title of the frozen tab.
 */
TEST_F('HungRendererDialogUITest', 'testTabTable', function() {
  var tabTable = $('tab-table');
  assertNotEquals(null, tabTable);
  assertGT(tabTable.childElementCount, 0);

  var children = tabTable.getElementsByTagName('*');
  assertNotEquals(null, children);
  var titleElement = children[0];
  assertNotEquals(null, titleElement);
  expectEquals(chrome.expectedTitle, titleElement.textContent);
});

