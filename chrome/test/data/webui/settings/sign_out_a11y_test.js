// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of accessibility tests for the SIGN_OUT route. */

// Do not test the SIGN_OUT route on Chrome OS since signing out is done at the
// OS level, not within the Chrome Browser.
GEN('#if !defined(OS_CHROMEOS)');

 /** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  ROOT_PATH + 'chrome/test/data/webui/settings/accessibility_browsertest.js',
]);

/**
 * Test fixture for SIGN_OUT
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsA11ySignOut() {}

SettingsA11ySignOut.prototype = {
  __proto__: SettingsAccessibilityTest.prototype,

  // Include files that define the mocha tests.
  extraLibraries: SettingsAccessibilityTest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'test_sync_browser_proxy.js',
    'sign_out_a11y_test.js',
  ]),
};

AccessibilityTest.define('SettingsA11ySignOut', {
  /** @override */
  name: 'SIGN_OUT',
  /** @type {SettingsPeoplePageElement}*/
  peoplePage: null,
  /** @type {TestSyncBrowserProxy}*/
  browserProxy:  null,
  /** @override */
  axeOptions: {
    'rules': {
      // TODO(hcarmona): enable 'region' after addressing violation.
      'region': {enabled: false},
      // Disable 'skip-link' check since there are few tab stops before the main
      // content.
      'skip-link': {enabled: false},
    }
  },
  /** @override */
  setup: function() {
    // Reset the blank to be a blank page.
    PolymerTest.clearBody();

    // Set the URL of the page to render to load the correct view upon
    // injecting settings-ui without attaching listeners.
    window.history.pushState(
        'object or string', 'Test', settings.routes.PEOPLE.path);

    this.browserProxy = new TestSyncBrowserProxy();
    settings.SyncBrowserProxyImpl.instance_ = this.browserProxy;

    var settingsUi = document.createElement('settings-ui');
    document.body.appendChild(settingsUi);
    Polymer.dom.flush();

    this.peoplePage = settingsUi.$$('settings-main').$$('settings-basic-page')
        .$$('settings-people-page');

    assert(!!this.peoplePage);
  },
  /** @override */
  tests: {
    'Accessible Dialog': function() {
      var disconnectButton = null;
      return this.browserProxy.getSyncStatus().then((syncStatus) => {
        // Navigate to the sign out dialog.
        Polymer.dom.flush();
        var disconnectButton = this.peoplePage.$$('#disconnectButton');
        assert(!!disconnectButton);
        MockInteractions.tap(disconnectButton);
        Polymer.dom.flush();
      });
    }
  },
  /** @override */
  violationFilter: {
    // TODO(quacht): remove this exception once the color contrast issue is
    // solved.
    // http://crbug.com/748608
    'color-contrast': function(nodeResult) {
      return nodeResult.element.id === 'prompt';
    },
    // Ignore errors caused by polymer aria-* attributes.
    'aria-valid-attr': function(nodeResult) {
      return nodeResult.element.hasAttribute('aria-active-attribute');
    },
  },
});

GEN('#endif  // defined(OS_CHROMEOS)');