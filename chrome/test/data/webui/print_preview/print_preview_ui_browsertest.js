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
  accessibilityIssuesAreErrors: true,

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
    ROOT_PATH + 'chrome/test/data/webui/test_browser_proxy.js',
    'print_preview_tests.js',
    'native_layer_stub.js',
  ],
};

// Run each mocha test in isolation (within a new TEST_F() call).
[
  'PrinterList',
  'PrinterListCloudEmpty',
  'RestoreLocalDestination',
  'RestoreMultipleDestinations',
  'DefaultDestinationSelectionRules',
  'SystemDialogLinkIsHiddenInAppKioskMode',
  'SectionsDisabled',
  'PrintToPDFSelectedCapabilities',
  'SourceIsHTMLCapabilities',
  'SourceIsPDFCapabilities',
  'ScalingUnchecksFitToPage',
  'CheckNumCopiesPrintPreset',
  'CheckDuplexPrintPreset',
  'CustomMarginsControlsCheck',
  'PageLayoutHasNoMarginsHideHeaderFooter',
  'PageLayoutHasMarginsShowHeaderFooter',
  'ZeroTopAndBottomMarginsHideHeaderFooter',
  'ZeroTopAndNonZeroBottomMarginShowHeaderFooter',
  'SmallPaperSizeHeaderFooter',
  'ColorSettingsMonochrome',
  'ColorSettingsCustomMonochrome',
  'ColorSettingsColor',
  'ColorSettingsCustomColor',
  'ColorSettingsBothStandardDefaultColor',
  'ColorSettingsBothStandardDefaultMonochrome',
  'ColorSettingsBothCustomDefaultColor',
  'DuplexSettingsTrue',
  'DuplexSettingsFalse',
  'PrinterChangeUpdatesPreview',
  'NoPDFPluginErrorMessage',
  'CustomPaperNames',
  'InitIssuesOneRequest',
  'InvalidSettingsError',
  'GenerateDraft',
].forEach(function(testName) {
  TEST_F('PrintPreviewUIBrowserTest', testName, function() {
    mocha.grep(new RegExp(testName + '\\b')).run();
  });
});


// Disable accessibility errors for some tests.
[
  'AdvancedSettings1Option',
  'AdvancedSettings2Options',
].forEach(function(testName) {
  TEST_F('PrintPreviewUIBrowserTest', testName, function() {
    this.accessibilityIssuesAreErrors = false;
    mocha.grep(new RegExp(testName + '\\b')).run();
  });
});

GEN('#if !defined(OS_CHROMEOS)');
TEST_F('PrintPreviewUIBrowserTest', 'SystemDefaultPrinterPolicy', function() {
  loadTimeData.overrideValues({useSystemDefaultPrinter: true});
  mocha.grep(new RegExp('SystemDefaultPrinterPolicy' + '\\b')).run();
});
GEN('#endif');
