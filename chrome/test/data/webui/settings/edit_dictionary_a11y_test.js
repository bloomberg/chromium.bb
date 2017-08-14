// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the EDIT_DICTIONARY route.
 */

// Disable since the EDIT_DICTIONARY route does not exist on Mac.
GEN('#if !defined(OS_MACOSX)');

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  ROOT_PATH + 'chrome/test/data/webui/settings/accessibility_browsertest.js',
]);

AccessibilityTest.define('SettingsAccessibilityTest', {
  /** @override */
  name: 'EDIT_DICTIONARY',
  /** @override */
  axeOptions: {
    'rules': {
      // TODO(hcarmona): enable 'region' after addressing violation.
      'region': {enabled: false},
      // Disable 'skip-link' check since there are few tab stops before the main
      // content.
      'skip-link': {enabled: false},
    }
  },
  /** @override */
  setup: function() {
    console.log('the route is not undefined!');
    assert(settings.routes.EDIT_DICTIONARY != undefined);
    settings.navigateTo(settings.routes.EDIT_DICTIONARY);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {
    'Accessible with No Changes': function() {}
  },
  /** @override */
  violationFilter: {
    // TODO(quacht): remove this exception once the color contrast issue is
    // solved.
    // http://crbug.com/748608
    'color-contrast': function(nodeResult) {
      return nodeResult.element.id === 'prompt';
    },
    'aria-valid-attr': function(nodeResult) {
      return nodeResult.element.hasAttribute('aria-active-attribute');
    },
    'aria-valid-attr-value': function(nodeResult) {
      var describerId = nodeResult.element.getAttribute('aria-describedby');
      return  describerId === 'cloudPrintersSecondary' ||
          (describerId === '' && nodeResult.element.id === 'input');
    },
    'button-name': function(nodeResult) {
      var node = nodeResult.element;
      return node.classList.contains('icon-expand-more');
    },
  },
});

GEN('#endif // !defined(OS_MACOSX)');