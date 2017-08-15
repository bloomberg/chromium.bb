// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the MANAGE_PROFILE route.
 */


// The MANAGE_PROFILE route is non-Chrome OS only.
GEN('#if !defined(OS_CHROMEOS)');

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  ROOT_PATH + 'chrome/test/data/webui/settings/accessibility_browsertest.js',
]);

// TODO(quacht): refactor to provide a default set of axeOptions and violation
// filters for settings accessibility tests.
AccessibilityTest.define('SettingsAccessibilityTest', {
  /** @override */
  name: 'MANAGE_PROFILE',
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
    settings.navigateTo(settings.routes.MANAGE_PROFILE);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {
    'Accessible with No Changes': function() {}
  },
  /** @override */
  violationFilter: {
    'aria-valid-attr': function(nodeResult) {
      return nodeResult.element.hasAttribute('aria-active-attribute');
    },
    // Excuse Polymer paper-input elements.
    'aria-valid-attr-value': function(nodeResult) {
      var describerId = nodeResult.element.getAttribute('aria-describedby');
      return describerId === '' && nodeResult.element.id === 'input';
    },
    'button-name': function(nodeResult) {
      var node = nodeResult.element;
      return node.classList.contains('icon-expand-more');
    },
  },
});

GEN('#endif  // !defined(OS_CHROMEOS)');