// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Print Preview interactive UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

const PrintPreviewInteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }

  /** @override */
  get extraLibraries() {
    return [
      ...super.extraLibraries,
      '//ui/webui/resources/js/assert.js',
    ];
  }

  // The name of the mocha suite. Should be overridden by subclasses.
  get suiteName() {
    return null;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};

PrintPreviewPrintHeaderInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/header.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//chrome/test/data/webui/settings/test_util.js',
      'print_header_interactive_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return print_header_interactive_test.suiteName;
  }
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_FocusPrintOnReady DISABLED_FocusPrintOnReady');
GEN('#else');
GEN('#define MAYBE_FocusPrintOnReady FocusPrintOnReady');
GEN('#endif');
TEST_F(
    'PrintPreviewPrintHeaderInteractiveTest', 'MAYBE_FocusPrintOnReady',
    function() {
      this.runMochaTest(
          print_header_interactive_test.TestNames.FocusPrintOnReady);
    });

PrintPreviewButtonStripInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/button_strip.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//chrome/test/data/webui/settings/test_util.js',
      'button_strip_interactive_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return button_strip_interactive_test.suiteName;
  }
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_FocusPrintOnReady DISABLED_FocusPrintOnReady');
GEN('#else');
GEN('#define MAYBE_FocusPrintOnReady FocusPrintOnReady');
GEN('#endif');
TEST_F(
    'PrintPreviewButtonStripInteractiveTest', 'MAYBE_FocusPrintOnReady',
    function() {
      this.runMochaTest(
          button_strip_interactive_test.TestNames.FocusPrintOnReady);
    });

PrintPreviewDestinationDialogInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/destination_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//chrome/test/data/webui/settings/test_util.js',
      '//ui/webui/resources/js/web_ui_listener_behavior.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'destination_dialog_interactive_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_dialog_interactive_test.suiteName;
  }
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_FocusSearchBox DISABLED_FocusSearchBox');
GEN('#else');
GEN('#define MAYBE_FocusSearchBox FocusSearchBox');
GEN('#endif');
TEST_F(
    'PrintPreviewDestinationDialogInteractiveTest', 'MAYBE_FocusSearchBox',
    function() {
      this.runMochaTest(
          destination_dialog_interactive_test.TestNames.FocusSearchBox);
    });

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_EscapeSearchBox DISABLED_EscapeSearchBox');
GEN('#else');
GEN('#define MAYBE_EscapeSearchBox EscapeSearchBox');
GEN('#endif');
TEST_F(
    'PrintPreviewDestinationDialogInteractiveTest', 'MAYBE_EscapeSearchBox',
    function() {
      this.runMochaTest(
          destination_dialog_interactive_test.TestNames.EscapeSearchBox);
    });

PrintPreviewPagesSettingsTest = class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/pages_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'pages_settings_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return pages_settings_test.suiteName;
  }
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_ClearInput DISABLED_ClearInput');
GEN('#else');
GEN('#define MAYBE_ClearInput ClearInput');
GEN('#endif');
TEST_F('PrintPreviewPagesSettingsTest', 'MAYBE_ClearInput', function() {
  this.runMochaTest(pages_settings_test.TestNames.ClearInput);
});

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_InputNotDisabledOnValidityChange DISABLED_InputNotDisabledOnValidityChange');
GEN('#else');
GEN('#define MAYBE_InputNotDisabledOnValidityChange InputNotDisabledOnValidityChange');
GEN('#endif');
TEST_F(
    'PrintPreviewPagesSettingsTest', 'MAYBE_InputNotDisabledOnValidityChange',
    function() {
      this.runMochaTest(
          pages_settings_test.TestNames.InputNotDisabledOnValidityChange);
    });

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_EnterOnInputTriggersPrint DISABLED_EnterOnInputTriggersPrint');
GEN('#else');
GEN('#define MAYBE_EnterOnInputTriggersPrint EnterOnInputTriggersPrint');
GEN('#endif');
TEST_F(
    'PrintPreviewPagesSettingsTest', 'MAYBE_EnterOnInputTriggersPrint',
    function() {
      this.runMochaTest(
          pages_settings_test.TestNames.EnterOnInputTriggersPrint);
    });

PrintPreviewNumberSettingsSectionInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/number_settings_section.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'number_settings_section_interactive_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return number_settings_section_interactive_test.suiteName;
  }
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_BlurResetsEmptyInput DISABLED_BlurResetsEmptyInput');
GEN('#else');
GEN('#define MAYBE_BlurResetsEmptyInput BlurResetsEmptyInput');
GEN('#endif');
TEST_F(
    'PrintPreviewNumberSettingsSectionInteractiveTest',
    'MAYBE_BlurResetsEmptyInput', function() {
      this.runMochaTest(number_settings_section_interactive_test.TestNames
                            .BlurResetsEmptyInput);
    });

PrintPreviewScalingSettingsInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/scaling_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/util.js',
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'scaling_settings_interactive_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return scaling_settings_interactive_test.suiteName;
  }
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_AutoFocusInput DISABLED_InputAutoFocus');
GEN('#else');
GEN('#define MAYBE_AutoFocusInput InputAutoFocus');
GEN('#endif');
TEST_F(
    'PrintPreviewScalingSettingsInteractiveTest', 'MAYBE_AutoFocusInput',
    function() {
      this.runMochaTest(
          scaling_settings_interactive_test.TestNames.AutoFocusInput);
    });
