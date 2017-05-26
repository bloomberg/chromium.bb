// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Print Preview tests. */

var ROOT_PATH = '../../../../../';

/**
 * @constructor
 * @extends {testing.Test}
 */
function PrintPreviewUIBrowserTest() {}

PrintPreviewUIBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the sample page, cause print preview & call preLoad().
   * @override
   */
  browsePrintPreload: 'print_preview/print_preview_hello_world_test.html',

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  accessibilityIssuesAreErrors: false,

  /** @override */
  isAsync: true,

  /** @override */
  preLoad: function() {
    window.isTest = true;
    testing.Test.prototype.preLoad.call(this);

  },

  /** @override */
  setUp: function() {
    testing.Test.prototype.setUp.call(this);

    testing.Test.disableAnimationsAndTransitions();
    // Enable when failure is resolved.
    // AX_TEXT_03: http://crbug.com/559209
    this.accessibilityAuditConfig.ignoreSelectors(
        'multipleLabelableElementsPerLabel',
        '#page-settings > .right-column > *');
  },

  extraLibraries: [
    ROOT_PATH + 'ui/webui/resources/js/cr.js',
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    ROOT_PATH + 'third_party/mocha/mocha.js',
    ROOT_PATH + 'chrome/test/data/webui/mocha_adapter.js',
    ROOT_PATH + 'ui/webui/resources/js/util.js',
    ROOT_PATH + 'chrome/test/data/webui/settings/test_browser_proxy.js',
    'print_preview_tests.js',
    'native_layer_stub.js',
  ],
};

TEST_F('PrintPreviewUIBrowserTest', 'PrinterList', function() {
  mocha.grep(/PrinterList\b/).run();
});

TEST_F('PrintPreviewUIBrowserTest', 'PrinterListCloudEmpty', function() {
  mocha.grep(/PrinterListCloudEmpty\b/).run();
});

TEST_F('PrintPreviewUIBrowserTest', 'RestoreLocalDestination', function() {
  mocha.grep(/RestoreLocalDestination\b/).run();
});

TEST_F('PrintPreviewUIBrowserTest', 'RestoreMultipleDestinations', function() {
  mocha.grep(/RestoreMultipleDestinations\b/).run();
});

TEST_F('PrintPreviewUIBrowserTest', 'DefaultDestinationSelectionRules',
    function() {
      mocha.grep(/DefaultDestinationSelectionRules\b/).run();
    });
