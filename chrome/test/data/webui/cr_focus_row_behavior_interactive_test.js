// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

/**
 * Test fixture for FocusRowBehavior.
 * @constructor
 * @extends {PolymerInteractiveUITest}
 */
function CrFocusRowBehaviorTest() {}

CrFocusRowBehaviorTest.prototype = {
  __proto__: PolymerInteractiveUITest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/html/cr/ui/focus_row_behavior.html',

  /** @override */
  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
    '//ui/webui/resources/js/util.js',
    'cr_focus_row_behavior_test.js',
    'settings/test_util.js',
  ],

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  },
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_FocusTest DISABLED_FocusTest');
GEN('#else');
GEN('#define MAYBE_FocusTest FocusTest');
GEN('#endif');
TEST_F('CrFocusRowBehaviorTest', 'MAYBE_FocusTest', function() {
  mocha.run();
});
