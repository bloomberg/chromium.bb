// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Settings tests. */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE([
    ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "base/command_line.h"');
GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_browsertest.h"');

/**
 * Test fixture for Polymer Settings elements.
 * @constructor
 * @extends {PolymerTest}
*/
function CrExtensionsBrowserTest() {}

CrExtensionsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://extensions/',

  /** @override */
  commandLineSwitches: [{
    switchName: 'enable-md-extensions',
  }],

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'extension_service_test.js',
  ]),

  /** @override */
  typedefCppFixture: 'ExtensionSettingsUIBrowserTest',
};

function CrExtensionsBrowserTestWithInstalledExtension() {}

CrExtensionsBrowserTestWithInstalledExtension.prototype = {
  __proto__: CrExtensionsBrowserTest.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('  InstallGoodExtension();');
    GEN('  SetAutoConfirmUninstall();');
  },
};

TEST_F('CrExtensionsBrowserTestWithInstalledExtension',
       'ExtensionServiceToggleEnableTest', function() {
  extension_service_tests.registerToggleEnableTests();
  mocha.run();
});
TEST_F('CrExtensionsBrowserTestWithInstalledExtension',
       'ExtensionServiceToggleIncognitoTest', function() {
  extension_service_tests.registerToggleIncognitoTests();
  mocha.run();
});
TEST_F('CrExtensionsBrowserTestWithInstalledExtension',
       'ExtensionServiceUninstallTest', function() {
  extension_service_tests.registerUninstallTests();
  mocha.run();
});
TEST_F('CrExtensionsBrowserTestWithInstalledExtension',
       'ExtensionServiceProfileSettingsTest', function() {
  extension_service_tests.registerProfileSettingsTests();
  mocha.run();
});
