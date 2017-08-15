// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the BASIC route.
 */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  ROOT_PATH + 'chrome/test/data/webui/settings/accessibility_browsertest.js',
]);

AccessibilityTest.define('SettingsAccessibilityTest', {
  /** @override */
  name: 'BASIC',
  /** @override */
  axeOptions: SettingsAccessibilityTest.axeOptions,
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter:
      Object.assign({}, SettingsAccessibilityTest.violationFilter, {
        'button-name': function(nodeResult) {
          var node = nodeResult.element;
          return node.classList.contains('icon-expand-more');
        },
      }),
});