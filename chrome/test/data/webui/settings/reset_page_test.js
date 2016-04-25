// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_reset_page', function() {
  /** @enum {string} */
  var TestNames = {
    PowerwashDialogAction: 'PowerwashDialogAction',
    PowerwashDialogOpenClose: 'PowerwashDialogOpenClose',
    ResetBannerClose: 'ResetBannerClose',
    ResetBannerReset: 'ResetBannerReset',
    ResetProfileDialogAction: 'ResetProfileDialogAction',
    ResetProfileDialogOpenClose: 'ResetProfileDialogOpenClose',
  };

  /**
   * @constructor
   * @implements {settings.ResetBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestResetBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'performResetProfileSettings',
      'onHideResetProfileDialog',
      'onHideResetProfileBanner',
      'onShowResetProfileDialog',
      'getReportedSettings',
      'onPowerwashDialogShow',
      'requestFactoryResetRestart',
    ]);
  };

  TestResetBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    performResetProfileSettings: function(sendSettings) {
      this.methodCalled('performResetProfileSettings');
      return Promise.resolve();
    },

    /** @override */
    onHideResetProfileDialog: function() {
      this.methodCalled('onHideResetProfileDialog');
    },

    /** @override */
    onHideResetProfileBanner: function() {
      this.methodCalled('onHideResetProfileBanner');
    },

    /** @override */
    onShowResetProfileDialog: function() {
      this.methodCalled('onShowResetProfileDialog');
    },

    /** @override */
    getReportedSettings: function() {
      this.methodCalled('getReportedSettings');
      return Promise.resolve([]);
    },

    /** @override */
    onPowerwashDialogShow: function() {
      this.methodCalled('onPowerwashDialogShow');
    },

    /** @override */
    requestFactoryResetRestart: function() {
      this.methodCalled('requestFactoryResetRestart');
    },
  };

  function registerBannerTests() {
    suite('BannerTests', function() {
      var resetBanner = null;
      var browserProxy = null;

      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://md-settings/reset_page/reset_profile_banner.html');
      });

      setup(function() {
        browserProxy = new TestResetBrowserProxy();
        settings.ResetBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        resetBanner = document.createElement('settings-reset-profile-banner');
        document.body.appendChild(resetBanner);
      });

      teardown(function() { resetBanner.remove(); });

      // Tests that the reset profile banner
      //  - opens the reset profile dialog when the reset button is clicked.
      //  - the reset profile dialog is closed after reset is done.
      test(TestNames.ResetBannerReset, function() {
        var dialog = resetBanner.$$('settings-reset-profile-dialog');
        assertFalse(!!dialog);
        MockInteractions.tap(resetBanner.$['reset']);
        Polymer.dom.flush();
        dialog = resetBanner.$$('settings-reset-profile-dialog');
        assertTrue(!!dialog);

        dialog.dispatchEvent(new CustomEvent('reset-done'));
        Polymer.dom.flush();
        assertEquals('none', dialog.style.display);
        return Promise.resolve();
      });

      // Tests that the reset profile banner removes itself from the DOM when
      // the close button is clicked and that |onHideResetProfileBanner| is
      // called.
      test(TestNames.ResetBannerClose, function() {
        MockInteractions.tap(resetBanner.$['close']);
        assertFalse(!!resetBanner.parentNode);
        return browserProxy.whenCalled('onHideResetProfileBanner');
      });
    });
  }

  function registerDialogTests() {
    suite('DialogTests', function() {
      var resetPage = null;

      setup(function() {
        browserProxy = new TestResetBrowserProxy();
        settings.ResetBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        resetPage = document.createElement('settings-reset-page');
        document.body.appendChild(resetPage);
      });

      teardown(function() { resetPage.remove(); });

      /**
       * @param {function(SettingsResetProfileDialogElemeent)}
       *     closeDialogFn A function to call for closing the dialog.
       * @return {!Promise}
       */
      function testOpenCloseResetProfileDialog(closeDialogFn) {
        browserProxy.resetResolver('onShowResetProfileDialog');
        browserProxy.resetResolver('onHideResetProfileDialog');

        // Open reset profile dialog.
        MockInteractions.tap(resetPage.$.resetProfile);
        var dialog = resetPage.$$('settings-reset-profile-dialog');
        assertTrue(!!dialog);
        var onDialogClosed = new Promise(
            function(resolve, reject) {
              dialog.addEventListener('iron-overlay-closed', resolve);
            });

        return browserProxy.whenCalled('onShowResetProfileDialog').then(
            function() {
              closeDialogFn(dialog);
              return Promise.all([
                onDialogClosed,
                browserProxy.whenCalled('onHideResetProfileDialog'),
              ]);
            });
      }

      // Tests that the reset profile dialog opens and closes correctly and that
      // browserProxy calls are occurring as expected.
      test(TestNames.ResetProfileDialogOpenClose, function() {
        return Promise.all([
          // Test case where the 'cancel' button is clicked.
          testOpenCloseResetProfileDialog(
              function(dialog) {
                MockInteractions.tap(dialog.$.cancel);
              }),
          // Test case where the 'close' button is clicked.
          testOpenCloseResetProfileDialog(
              function(dialog) {
                MockInteractions.tap(dialog.$.dialog.getCloseButton());
              }),
          // Test case where the 'Esc' key is pressed.
          testOpenCloseResetProfileDialog(
              function(dialog) {
                MockInteractions.pressAndReleaseKeyOn(
                    dialog, 27 /* 'Esc' key code */);
              }),
        ]);
      });

      // Tests that when user request to reset the profile the appropriate
      // message is sent to the browser.
      test(TestNames.ResetProfileDialogAction, function() {
        // Open reset profile dialog.
        MockInteractions.tap(resetPage.$.resetProfile);
        var dialog = resetPage.$$('settings-reset-profile-dialog');
        assertTrue(!!dialog);

        var showReportedSettingsLink = dialog.$$('.footer a');
        assertTrue(!!showReportedSettingsLink);
        MockInteractions.tap(showReportedSettingsLink);

        return browserProxy.whenCalled('getReportedSettings').then(function() {
          MockInteractions.tap(dialog.$.reset);
          return browserProxy.whenCalled('performResetProfileSettings');
        });
      });

      if (cr.isChromeOS) {
        /**
         * @param {function(SettingsPowerwashDialogElemeent):!Element}
         *     closeButtonFn A function that returns the button to be used for
         *     closing the dialog.
         * @return {!Promise}
         */
        function testOpenClosePowerwashDialog(closeButtonFn) {
          // Open powerwash dialog.
          MockInteractions.tap(resetPage.$.powerwash);
          var dialog = resetPage.$$('settings-powerwash-dialog');
          assertTrue(!!dialog);
          var onDialogClosed = new Promise(
              function(resolve, reject) {
                dialog.addEventListener('iron-overlay-closed', resolve);
              });

          MockInteractions.tap(closeButtonFn(dialog));
          return Promise.all([
              onDialogClosed,
              browserProxy.whenCalled('onPowerwashDialogShow'),
          ]);
        }

        // Tests that the powerwash dialog opens and closes correctly, and
        // that chrome.send calls are propagated as expected.
        test(TestNames.PowerwashDialogOpenClose, function() {
          return Promise.all([
            // Test case where the 'cancel' button is clicked.
            testOpenClosePowerwashDialog(
                function(dialog) { return dialog.$.cancel; }),
            // Test case where the 'close' button is clicked.
            testOpenClosePowerwashDialog(
                function(dialog) { return dialog.$.dialog.getCloseButton(); }),
          ]);
        });

        // Tests that when powerwash is requested chrome.send calls are
        // propagated as expected.
        test(TestNames.PowerwashDialogAction, function() {
          // Open powerwash dialog.
          MockInteractions.tap(resetPage.$.powerwash);
          var dialog = resetPage.$$('settings-powerwash-dialog');
          assertTrue(!!dialog);
          MockInteractions.tap(dialog.$.powerwash);
          return browserProxy.whenCalled('requestFactoryResetRestart');
        });
      }
    });
  }

  return {
    registerTests: function() {
      registerBannerTests();
      registerDialogTests();
    },
  };
});
