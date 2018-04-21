// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for MultiDevice unified setup WebUI. */

GEN('#if defined(OS_CHROMEOS)');

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for MultiDeviceSetup elements.
 * @constructor
 * @extends {PolymerTest}
 */
function MultiDeviceSetupBrowserTest() {}

MultiDeviceSetupBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://multidevice-setup/',

  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'navigation_test.js',
  ]),
};

TEST_F('MultiDeviceSetupBrowserTest', 'All', function() {
  multidevice_setup.registerTests();
  mocha.run();
});

GEN('#endif');
