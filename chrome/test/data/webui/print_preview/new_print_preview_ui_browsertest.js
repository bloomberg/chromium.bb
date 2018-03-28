// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Print Preview tests for the new UI. */

const ROOT_PATH = '../../../../../';

GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/common/chrome_features.h"');

function PrintPreviewSettingsSectionsTest() {}

const NewPrintPreviewTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/';
  }

  /** @override */
  get featureList() {
    return ['features::kNewPrintPreview', ''];
  }

  /** @override */
  get extraLibraries() {
    return PolymerTest.getLibraries(ROOT_PATH).concat([
      ROOT_PATH + 'ui/webui/resources/js/assert.js',
    ]);
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

PrintPreviewSettingsSectionsTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      ROOT_PATH + 'chrome/test/data/webui/settings/test_util.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'settings_section_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return settings_sections_tests.suiteName;
  }
};

TEST_F('PrintPreviewSettingsSectionsTest', 'Copies', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Copies);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Layout', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Layout);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Color', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Color);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'MediaSize', function() {
  this.runMochaTest(settings_sections_tests.TestNames.MediaSize);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Margins', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Margins);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Dpi', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Dpi);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Scaling', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Scaling);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Other', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Other);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetPages', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetPages);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetCopies', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetCopies);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetLayout', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetLayout);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetColor', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetColor);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetMediaSize', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetMediaSize);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetDpi', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetDpi);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetMargins', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetMargins);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetScaling', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetScaling);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetOther', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetOther);
});

PrintPreviewRestoreStateTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'restore_state_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return restore_state_test.suiteName;
  }
};

TEST_F('PrintPreviewRestoreStateTest', 'RestoreTrueValues', function() {
  this.runMochaTest(restore_state_test.TestNames.RestoreTrueValues);
});

TEST_F('PrintPreviewRestoreStateTest', 'RestoreFalseValues', function() {
  this.runMochaTest(restore_state_test.TestNames.RestoreFalseValues);
});

TEST_F('PrintPreviewRestoreStateTest', 'SaveValues', function() {
  this.runMochaTest(restore_state_test.TestNames.SaveValues);
});

PrintPreviewModelTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/model.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'model_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return model_test.suiteName;
  }
};

TEST_F('PrintPreviewModelTest', 'SetStickySettings', function() {
  this.runMochaTest(model_test.TestNames.SetStickySettings);
});

PrintPreviewPreviewGenerationTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'preview_generation_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return preview_generation_test.suiteName;
  }
};

TEST_F('PrintPreviewPreviewGenerationTest', 'Color', function() {
  this.runMochaTest(preview_generation_test.TestNames.Color);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'CssBackground', function() {
  this.runMochaTest(preview_generation_test.TestNames.CssBackground);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'FitToPage', function() {
  this.runMochaTest(preview_generation_test.TestNames.FitToPage);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'HeaderFooter', function() {
  this.runMochaTest(preview_generation_test.TestNames.HeaderFooter);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'Layout', function() {
  this.runMochaTest(preview_generation_test.TestNames.Layout);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'Margins', function() {
  this.runMochaTest(preview_generation_test.TestNames.Margins);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'MediaSize', function() {
  this.runMochaTest(preview_generation_test.TestNames.MediaSize);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'PageRange', function() {
  this.runMochaTest(preview_generation_test.TestNames.PageRange);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'SelectionOnly', function() {
  this.runMochaTest(preview_generation_test.TestNames.SelectionOnly);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'Scaling', function() {
  this.runMochaTest(preview_generation_test.TestNames.Scaling);
});

GEN('#if !defined(OS_WIN) && !defined(OS_MACOSX)');
TEST_F('PrintPreviewPreviewGenerationTest', 'Rasterize', function() {
  this.runMochaTest(preview_generation_test.TestNames.Rasterize);
});
GEN('#endif');

TEST_F('PrintPreviewPreviewGenerationTest', 'Destination', function() {
  this.runMochaTest(preview_generation_test.TestNames.Destination);
});
