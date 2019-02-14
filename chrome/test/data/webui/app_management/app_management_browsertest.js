// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the App Management page.
 */
const ROOT_PATH = '../../../../../';

GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/common/chrome_features.h"');

function AppManagementBrowserTest() {}

AppManagementBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://apps',

  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    '../test_store.js',
    'test_util.js',
    'test_store.js',
  ]),

  featureList: ['features::kAppManagement', ''],

  /** override */
  runAccessibilityChecks: true,
};

function AppManagementAppTest() {}

AppManagementAppTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'app_test.js',
  ]),
};

TEST_F('AppManagementAppTest', 'All', function() {
  mocha.run();
});

function AppManagementDomSwitchTest() {}

AppManagementDomSwitchTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'dom_switch_test.js',
  ]),
};

TEST_F('AppManagementDomSwitchTest', 'All', function() {
  mocha.run();
});

function AppManagementMainViewTest() {}

AppManagementMainViewTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'main_view_test.js',
  ]),
};

TEST_F('AppManagementMainViewTest', 'All', function() {
  mocha.run();
});

function AppManagementMetadataViewTest() {}

AppManagementMetadataViewTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'metadata_view_test.js',
  ]),
};

TEST_F('AppManagementMetadataViewTest', 'All', function() {
  mocha.run();
});

function AppManagementReducersTest() {}

AppManagementReducersTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'reducers_test.js',
  ]),
};

TEST_F('AppManagementReducersTest', 'All', function() {
  mocha.run();
});

function AppManagementRouterTest() {}

AppManagementRouterTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'router_test.js',
  ]),
};

TEST_F('AppManagementRouterTest', 'All', function() {
  mocha.run();
});

function AppManagementPwaPermissionViewTest() {}

AppManagementPwaPermissionViewTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'pwa_permission_view_test.js',
  ]),
};

TEST_F('AppManagementPwaPermissionViewTest', 'All', function() {
  mocha.run();
});
