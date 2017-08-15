// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the EDIT_DICTIONARY route.
 */

// Disable since the EDIT_DICTIONARY route does not exist on Mac.
GEN('#if !defined(OS_MACOSX)');

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  'settings_accessibility_test.js',
]);

AccessibilityTest.define('SettingsAccessibilityTest', {
  /** @override */
  name: 'EDIT_DICTIONARY',
  /** @override */
  axeOptions: SettingsAccessibilityTest.axeOptions,
  /** @override */
  setup: function() {
    console.log('the route is not undefined!');
    assert(settings.routes.EDIT_DICTIONARY != undefined);
    settings.navigateTo(settings.routes.EDIT_DICTIONARY);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter:
      Object.assign({}, SettingsAccessibilityTest.violationFilter, {
        // Excuse Polymer paper-input elements.
        'aria-valid-attr-value': function(nodeResult) {
          var describerId = nodeResult.element.getAttribute('aria-describedby');
          return describerId === '' && nodeResult.element.id === 'input';
        },
        'button-name': function(nodeResult) {
          var node = nodeResult.element;
          return node.classList.contains('icon-expand-more');
        },
      })
});

GEN('#endif // !defined(OS_MACOSX)');