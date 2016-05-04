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
 * Basic test fixture for the MD chrome://extensions page. Installs no
 * extensions.
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
    'extension_test_util.js',
    'extension_item_test.js',
    'extension_item_list_test.js',
    'extension_service_test.js',
    'extension_sidebar_test.js',
    'extension_manager_test.js',
    '../mock_controller.js',
    '../../../../../ui/webui/resources/js/webui_resource_test.js',
  ]),

  /** @override */
  typedefCppFixture: 'ExtensionSettingsUIBrowserTest',
};

/**
 * Test fixture with one installed extension.
 * @constructor
 * @extends {CrExtensionsBrowserTest}
 */
function CrExtensionsBrowserTestWithInstalledExtension() {}

CrExtensionsBrowserTestWithInstalledExtension.prototype = {
  __proto__: CrExtensionsBrowserTest.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('  InstallGoodExtension();');
    GEN('  SetAutoConfirmUninstall();');
  },
};

/**
 * Test fixture with multiple installed extensions of different types.
 * @constructor
 * @extends {CrExtensionsBrowserTest}
 */
function CrExtensionsBrowserTestWithMultipleExtensionTypesInstalled() {}

CrExtensionsBrowserTestWithMultipleExtensionTypesInstalled.prototype = {
  __proto__: CrExtensionsBrowserTest.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('  InstallGoodExtension();');
    GEN('  InstallPackagedApp();');
    GEN('  InstallHostedApp();');
    GEN('  InstallPlatformApp();');
  },
};

////////////////////////////////////////////////////////////////////////////////
// Extension Sidebar Tests

TEST_F('CrExtensionsBrowserTest', 'ExtensionSidebarLayoutTest', function() {
  extension_sidebar_tests.registerTests();
  mocha.grep(assert(extension_sidebar_tests.TestNames.Layout)).run();
});
TEST_F('CrExtensionsBrowserTest', 'ExtensionSidebarClickHandlerTest',
       function() {
  extension_sidebar_tests.registerTests();
  mocha.grep(assert(extension_sidebar_tests.TestNames.ClickHandlers)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Item Tests

TEST_F('CrExtensionsBrowserTest', 'ExtensionItemNormalStateTest', function() {
  extension_item_tests.registerTests();
  var TestNames = extension_item_tests.TestNames;
  mocha.grep(assert(TestNames.ElementVisibilityNormalState)).run();
});

TEST_F('CrExtensionsBrowserTest', 'ExtensionItemDetailStateTest', function() {
  extension_item_tests.registerTests();
  var TestNames = extension_item_tests.TestNames;
  mocha.grep(assert(TestNames.ElementVisibilityDetailState)).run();
});

TEST_F('CrExtensionsBrowserTest', 'ExtensionItemDeveloperStateTest',
       function() {
  extension_item_tests.registerTests();
  var TestNames = extension_item_tests.TestNames;
  mocha.grep(assert(TestNames.ElementVisibilityDeveloperState)).run();
});

TEST_F('CrExtensionsBrowserTest', 'ExtensionItemClickableItemsTest',
       function() {
  extension_item_tests.registerTests();
  var TestNames = extension_item_tests.TestNames;
  mocha.grep(assert(TestNames.ClickableItems)).run();
});

TEST_F('CrExtensionsBrowserTest', 'ExtensionItemWarningsTest',
       function() {
  extension_item_tests.registerTests();
  mocha.grep(assert(extension_item_tests.TestNames.Warnings)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Item List Tests

TEST_F('CrExtensionsBrowserTest', 'ExtensionItemList', function() {
  extension_item_list_tests.registerTests();
  mocha.grep(
      assert(extension_item_list_tests.TestNames.ItemListFiltering)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Service Tests

TEST_F('CrExtensionsBrowserTestWithInstalledExtension',
       'ExtensionServiceToggleEnableTest', function() {
  extension_service_tests.registerTests();
  mocha.grep(assert(extension_service_tests.TestNames.EnableAndDisable)).run();
});
TEST_F('CrExtensionsBrowserTestWithInstalledExtension',
       'ExtensionServiceToggleIncognitoTest', function() {
  extension_service_tests.registerTests();
  mocha.grep(
      assert(extension_service_tests.TestNames.ToggleIncognitoMode)).run();
});
TEST_F('CrExtensionsBrowserTestWithInstalledExtension',
       'ExtensionServiceUninstallTest', function() {
  extension_service_tests.registerTests();
  mocha.grep(assert(extension_service_tests.TestNames.Uninstall)).run();
});
TEST_F('CrExtensionsBrowserTestWithInstalledExtension',
       'ExtensionServiceProfileSettingsTest', function() {
  extension_service_tests.registerTests();
  mocha.grep(assert(extension_service_tests.TestNames.ProfileSettings)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Manager Tests

TEST_F('CrExtensionsBrowserTest', 'ExtensionManagerItemOrderTest', function() {
  extension_manager_tests.registerTests();
  mocha.grep(assert(extension_manager_tests.TestNames.ItemOrder)).run();
});

TEST_F('CrExtensionsBrowserTestWithMultipleExtensionTypesInstalled',
       'ExtensionManagerItemListVisibilityTest', function() {
  extension_manager_tests.registerTests();
  mocha.grep(
      assert(extension_manager_tests.TestNames.ItemListVisibility)).run();
});

TEST_F('CrExtensionsBrowserTestWithMultipleExtensionTypesInstalled',
       'ExtensionManagerShowItemsTest', function() {
  extension_manager_tests.registerTests();
  mocha.grep(assert(extension_manager_tests.TestNames.ShowItems)).run();
});
