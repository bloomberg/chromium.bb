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
      'print_preview_tests.js',
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
