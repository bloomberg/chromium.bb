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
    'test_browser_proxy.js',
  ]),

  /** @override */
  preLoad: function() {
    SettingsPageBrowserTest.prototype.preLoad.call(this);
    settingsHidePagesByDefaultForTest = true;
  },
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
      'getTurnOffFlowStatus',
      'startTurnOffFlow',
      'cancelTurnOffFlow',
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

    /** @override */
    getTurnOffFlowStatus: function() {
      this.methodCalled('getTurnOffFlowStatus');
      return Promise.resolve('idle');
    },

    /** @override */
    startTurnOffFlow: function() {
      this.methodCalled('startTurnOffFlow');
    },

    /** @override */
    cancelTurnOffFlow: function() {
      this.methodCalled('cancelTurnOffFlow');
    },
  };

  /** @type {?SettingsPeoplePageElement} */
  var page = null;

  /** @type {?TestEasyUnlockBrowserProxy} */
  var browserProxy = null;

  /** @type {?CrSettingsPrefs} */
  var prefs = null;

  var self = this;

  suite('SettingsEasyUnlock', function() {
    suiteSetup(function() {
      self.getPage('basic').set('pageVisibility.people', true);
      Polymer.dom.flush();

      // These overrides are necessary for this test to function on ChromeOS
      // bots that do not have Bluetooth (don't actually support Easy Unlock).
      loadTimeData.overrideValues({
        easyUnlockAllowed: true,
        easyUnlockProximityDetectionAllowed: false,

        easyUnlockSectionTitle: '',
        easyUnlockLearnMoreURL: '',
        easyUnlockSetupIntro: '',
        easyUnlockSetupButton: '',

        easyUnlockDescription: '',
        easyUnlockTurnOffTitle: '',
        easyUnlockTurnOffDescription: '',
        easyUnlockTurnOffButton: '',
      });

      // Before clearing the body, save a copy of the real prefs so we can
      // cleanly re-create the People page element.
      prefs = document.querySelector('settings-ui').$$('settings-prefs').prefs;
    });

    setup(function() {
      browserProxy = new TestEasyUnlockBrowserProxy();
      settings.EasyUnlockBrowserProxyImpl.instance_ = browserProxy;

      PolymerTest.clearBody();
      page = document.createElement('settings-people-page');
      page.currentRoute = {
        page: 'basic',
        section: '',
        subpage: [],
      };
      page.prefs = prefs;
    });

    test('setup button', function() {
      document.body.appendChild(page);

      return browserProxy.whenCalled('getEnabledStatus').then(function() {
        assertTrue(page.easyUnlockAllowed_);
        expectFalse(page.easyUnlockEnabled_);

        Polymer.dom.flush();

        var setupButton = page.$$('#easyUnlockSetup');
        assertTrue(!!setupButton);
        expectFalse(setupButton.hidden);

        MockInteractions.tap(setupButton);
        return browserProxy.whenCalled('startTurnOnFlow');
      });
    });

    test('turn off dialog', function() {
      browserProxy.setEnabledStatus(true);
      document.body.appendChild(page);

      var turnOffDialog = null;

      return browserProxy.whenCalled('getEnabledStatus').then(function() {
        assertTrue(page.easyUnlockAllowed_);
        expectTrue(page.easyUnlockEnabled_);

        Polymer.dom.flush();

        var turnOffButton = page.$$('#easyUnlockTurnOff');
        assertTrue(!!turnOffButton);
        expectFalse(turnOffButton.hidden)

        MockInteractions.tap(turnOffButton);
        return browserProxy.whenCalled('getTurnOffFlowStatus');
      }).then(function() {
        Polymer.dom.flush();

        turnOffDialog = page.$$('#easyUnlockTurnOffDialog');
        assertTrue(!!turnOffDialog);

        // Verify that elements on the turn off dialog are hidden or active
        // according to the easy unlock turn off status.
        var turnOffDialogButtonContainer =
            turnOffDialog.$$('.button-container');
        var turnOffDialogButtonSpinner = turnOffDialog.$$('paper-spinner');
        var turnOffDialogConfirmButton = turnOffDialog.$$('#turnOff');
        var turnOffDialogCancelButton = turnOffDialog.$$('.cancel-button');
        assertTrue(!!turnOffDialogButtonContainer);
        assertTrue(!!turnOffDialogButtonSpinner);
        assertTrue(!!turnOffDialogConfirmButton);
        assertTrue(!!turnOffDialogCancelButton);

        cr.webUIListenerCallback('easy-unlock-turn-off-flow-status', 'offline');
        expectTrue(turnOffDialogButtonContainer.hidden);
        expectFalse(turnOffDialogButtonSpinner.active);

        cr.webUIListenerCallback('easy-unlock-turn-off-flow-status', 'pending');
        expectFalse(turnOffDialogButtonContainer.hidden);
        expectTrue(turnOffDialogButtonSpinner.active);

        cr.webUIListenerCallback('easy-unlock-turn-off-flow-status',
            'server-error');
        expectFalse(turnOffDialogButtonContainer.hidden);
        expectTrue(turnOffDialogCancelButton.hidden);

        cr.webUIListenerCallback('easy-unlock-turn-off-flow-status', 'idle');
        expectFalse(turnOffDialogConfirmButton.hidden);

        MockInteractions.tap(turnOffDialogConfirmButton);

        return browserProxy.whenCalled('startTurnOffFlow');
      }).then(function() {
        // To signal successful turnoff, the enabled status is broadcast
        // as false. At that point, the dialog should close and cancel
        // any in-progress turnoff flow. The cancellation should be
        // a no-op assuming the turnoff originated from this tab.
        cr.webUIListenerCallback('easy-unlock-enabled-status', false);
        return browserProxy.whenCalled('cancelTurnOffFlow');
      }).then(function() {
        Polymer.dom.flush();
        expectFalse(turnOffDialog.$.dialog.open);
      });
    });
  });

  // Run all registered tests.
  mocha.run();
});
