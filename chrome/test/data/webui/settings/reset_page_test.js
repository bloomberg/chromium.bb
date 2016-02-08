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
   * @param {string} name chrome.send message name.
   * @return {!Promise} Fires when chrome.send is called with the given message
   *     name.
   */
  function whenChromeSendCalled(name) {
    return new Promise(function(resolve, reject) {
      registerMessageCallback(name, null, resolve);
    });
  }

  function registerBannerTests() {
    suite('BannerTests', function() {
      var resetBanner = null;

      suiteSetup(function() {
        return Promise.all([
          PolymerTest.importHtml('chrome://md-settings/i18n_setup.html'),
          PolymerTest.importHtml(
              'chrome://md-settings/reset_page/reset_profile_banner.html')
        ]);
      });

      setup(function() {
        PolymerTest.clearBody();
        resetBanner = document.createElement('settings-reset-profile-banner');
        document.body.appendChild(resetBanner);
      });

      // Tests that the reset profile banner
      //  - opens the reset profile dialog when the reset button is clicked.
      //  - the reset profile dialog is closed after reset is done.
      test(TestNames.ResetBannerReset, function() {
        var dialog = resetBanner.$$('settings-reset-profile-dialog');
        assert(!dialog);
        MockInteractions.tap(resetBanner.$['reset']);
        Polymer.dom.flush();
        dialog = resetBanner.$$('settings-reset-profile-dialog');
        assertNotEquals(undefined, dialog);

        dialog.dispatchResetDoneEvent();
        Polymer.dom.flush();
        assertEquals('none', dialog.style.display);
        return Promise.resolve();
      });

      // Tests that the reset profile banner removes itself from the DOM when
      // the close button is clicked and that
      // chrome.send('onHideResetProfileBanner') is called.
      test(TestNames.ResetBannerClose, function() {
        var whenOnHideResetProfileBanner = whenChromeSendCalled(
            'onHideResetProfileBanner');
        MockInteractions.tap(resetBanner.$['close']);
        assert(!resetBanner.parentNode);
        return whenOnHideResetProfileBanner;
      });
    });
  }

  function registerDialogTests() {
    suite('DialogTests', function() {
      var resetPage = null;

      suiteSetup(function() {
        return Promise.all([
          PolymerTest.importHtml('chrome://md-settings/i18n_setup.html'),
          PolymerTest.importHtml(
              'chrome://md-settings/reset_page/reset_page.html')
        ]);
      });

      setup(function() {
        PolymerTest.clearBody();
        resetPage = document.createElement('settings-reset-page');
        document.body.appendChild(resetPage);
      });


      /**
       * @param {string} closeButtonId The ID of the button that closes the
       *     dialog.
       * @return {!Promise}
       */
      function testOpenCloseResetProfileDialog(closeButtonId) {
        var onShowResetProfileDialogCalled = whenChromeSendCalled(
            'onShowResetProfileDialog');
        var onHideResetProfileDialogCalled = whenChromeSendCalled(
            'onHideResetProfileDialog');

        // Open reset profile dialog.
        MockInteractions.tap(resetPage.$.resetProfile);
        var dialog = resetPage.$$('settings-reset-profile-dialog');
        var onDialogClosed = new Promise(
            function(resolve, reject) {
              dialog.addEventListener('iron-overlay-closed', resolve);
            });

        MockInteractions.tap(dialog.$[closeButtonId]);

        return Promise.all([
          onShowResetProfileDialogCalled,
          onHideResetProfileDialogCalled,
          onDialogClosed
        ]);
      }

      // Tests that the reset profile dialog opens and closes correctly and that
      // chrome.send calls are propagated as expected.
      test(TestNames.ResetProfileDialogOpenClose, function() {
        return Promise.all([
          // Test case where the 'cancel' button is clicked.
          testOpenCloseResetProfileDialog('cancel'),
          // Test case where the 'close' button is clicked.
          testOpenCloseResetProfileDialog('close')
        ]);
      });

      // Tests that when resetting the profile is requested chrome.send calls
      // are propagated as expected.
      test(TestNames.ResetProfileDialogAction, function() {
        // Open reset profile dialog.
        MockInteractions.tap(resetPage.$.resetProfile);
        var dialog = resetPage.$$('settings-reset-profile-dialog');
        var promise = whenChromeSendCalled('performResetProfileSettings');
        MockInteractions.tap(dialog.$.reset);
        return promise;
      });

      if (cr.isChromeOS) {
        /**
         * @param {string} closeButtonId The ID of the button that closes the
         *     dialog.
         * @return {!Promise}
         */
        function testOpenClosePowerwashDialog(closeButtonId) {
          var onPowerwashDialogShowCalled = whenChromeSendCalled(
              'onPowerwashDialogShow');

          // Open powerwash dialog.
          MockInteractions.tap(resetPage.$.powerwash);
          var dialog = resetPage.$$('settings-powerwash-dialog');
          var onDialogClosed = new Promise(
              function(resolve, reject) {
                dialog.addEventListener('iron-overlay-closed', resolve);
              });

          MockInteractions.tap(dialog.$[closeButtonId]);
          return Promise.all([onPowerwashDialogShowCalled, onDialogClosed]);
        }

        // Tests that the powerwash dialog opens and closes correctly, and
        // that chrome.send calls are propagated as expected.
        test(TestNames.PowerwashDialogOpenClose, function() {
          return Promise.all([
            // Test case where the 'cancel' button is clicked.
            testOpenClosePowerwashDialog('cancel'),
            // Test case where the 'close' button is clicked.
            testOpenClosePowerwashDialog('close')
          ]);
        });

        // Tests that when powerwash is requested chrome.send calls are
        // propagated as expected.
        test(TestNames.PowerwashDialogAction, function() {
          // Open powerwash dialog.
          MockInteractions.tap(resetPage.$.powerwash);
          var dialog = resetPage.$$('settings-powerwash-dialog');
          var promise = whenChromeSendCalled('requestFactoryResetRestart');
          MockInteractions.tap(dialog.$.powerwash);
          return promise;
        });
      }
    });
  }

  return {
    registerBannerTests: registerBannerTests,
    registerDialogTests: registerDialogTests
  };
});
