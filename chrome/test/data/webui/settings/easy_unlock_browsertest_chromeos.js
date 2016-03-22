// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for Easy Unlock within People section. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
*/
function SettingsEasyUnlockBrowserTest() {
}

SettingsEasyUnlockBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    'test_browser_proxy.js',
  ]),
};

// Times out on debug builders and may time out on memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434.
GEN('#if defined(MEMORY_SANITIZER) || !defined(NDEBUG)');
GEN('#define MAYBE_EasyUnlock DISABLED_EasyUnlock');
GEN('#else');
GEN('#define MAYBE_EasyUnlock EasyUnlock');
GEN('#endif');

// Runs change picture tests.
TEST_F('SettingsEasyUnlockBrowserTest', 'MAYBE_EasyUnlock', function() {
  /**
   * A test version of EasyUnlockBrowserProxy. Provides helper methods
   * for allowing tests to know when a method was called, as well as
   * specifying mock responses.
   *
   * @constructor
   * @implements {settings.EasyUnlockBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestEasyUnlockBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'getEnabledStatus',
      'startTurnOnFlow',
    ]);

    /** @private {boolean} */
    this.isEnabled_ = false;
  };

  TestEasyUnlockBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /**
     * @param {boolean} easyUnlockEnabled
     */
    setEnabledStatus: function(easyUnlockEnabled) {
      this.isEnabled_ = easyUnlockEnabled;
    },

    /** @override */
    getEnabledStatus: function() {
      this.methodCalled('getEnabledStatus');
      return Promise.resolve(this.isEnabled_);
    },

    /** @override */
    startTurnOnFlow: function() {
      this.methodCalled('startTurnOnFlow');
    },
  };

  /** @type {?SettingsPeoplePageElement} */
  var page = null;

  /** @type {?TestEasyUnlockBrowserProxy} */
  var browserProxy = null;

  suite('SettingsEasyUnlock', function() {
    setup(function() {
      // These overrides are necessary for this test to function on ChromeOS
      // bots that do not have Bluetooth (don't actually support Easy Unlock).
      loadTimeData.overrideValues({
        easyUnlockAllowed: true,
        easyUnlockProximityDetectionAllowed: false,

        easyUnlockSectionTitle: '',
        easyUnlockLearnMoreURL: '',
        easyUnlockSetupIntro: '',
        easyUnlockSetupButton: '',
      });

      browserProxy = new TestEasyUnlockBrowserProxy();
      settings.EasyUnlockBrowserProxyImpl.instance_ = browserProxy;

      // Before clearing the body, save a copy of the real prefs so we can
      // cleanly re-create the People page element.
      var prefs =
          document.querySelector('cr-settings').$$('settings-prefs').prefs;

      PolymerTest.clearBody();
      page = document.createElement('settings-people-page');
      page.currentRoute = {
        page: 'basic',
        section: '',
        subpage: [],
      };
      page.prefs = prefs;

      document.body.appendChild(page);
    });

    test('setup button', function() {
      return browserProxy.whenCalled('getEnabledStatus').then(function() {
        assertTrue(page.easyUnlockAllowed_);
        expectFalse(page.easyUnlockEnabled_);

        Polymer.dom.flush();

        var setupButton = page.$$('#easyUnlockSetup');
        assertTrue(!!setupButton);

        MockInteractions.tap(setupButton);
        return browserProxy.whenCalled('startTurnOnFlow');
      });
    });
  });

  // Run all registered tests.
  mocha.run();
});
