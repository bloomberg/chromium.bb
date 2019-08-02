// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements which rely on focus. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

// eslint-disable-next-line no-var
var CrElementsV3FocusTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://dummyurl';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

// eslint-disable-next-line no-var
var CrElementsCheckboxV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_checkbox_test.m.js';
  }
};

TEST_F('CrElementsCheckboxV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToggleV3Test = class extends CrElementsV3FocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_toggle_test.m.js';
  }
};

TEST_F('CrElementsToggleV3Test', 'All', function() {
  mocha.run();
});
