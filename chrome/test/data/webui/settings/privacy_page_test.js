// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_privacy_page', function() {
  /**
   * @constructor
   * @extends {TestBrowserProxy}
   * @implements {settings.PrivacyPageBrowserProxy}
   */
  function TestPrivacyPageBrowserProxy() {
    settings.TestBrowserProxy.call(this, ['showManageSSLCertificates']);
  }

  TestPrivacyPageBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    showManageSSLCertificates: function() {
      this.methodCalled('showManageSSLCertificates');
    },
  };

  /**
   * @constructor
   * @extends {TestBrowserProxy}
   * @implements {settings.ClearBrowsingDataBrowserProxy}
   */
  function TestClearBrowsingDataBrowserProxy() {
    settings.TestBrowserProxy.call(this, [
      'initialize',
      'clearBrowsingData',
    ]);

    /**
     * The promise to return from |clearBrowsingData|.
     * Allows testing code to test what happens after the call is made, and
     * before the browser responds.
     * @private {?Promise}
     */
    this.clearBrowsingDataPromise_ = null;
  }

  TestClearBrowsingDataBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @param {!Promise} promise */
    setClearBrowsingDataPromise: function(promise) {
      this.clearBrowsingDataPromise_ = promise;
    },

    /** @override */
    clearBrowsingData: function() {
      this.methodCalled('clearBrowsingData');
      return this.clearBrowsingDataPromise_ !== null ?
          this.clearBrowsingDataPromise_ : Promise.resolve();
    },

    /** @override */
    initialize: function() {
      this.methodCalled('initialize');
    },
  };

  function registerNativeCertificateManagerTests() {
    suite('NativeCertificateManager', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      var testBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      var page;

      suiteSetup(function() {
        settings.main.rendered = Promise.resolve();
      });

      setup(function() {
        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
      });

      teardown(function() { page.remove(); });

      test('NativeCertificateManager', function() {
        MockInteractions.tap(page.$.manageCertificates);
        return testBrowserProxy.whenCalled('showManageSSLCertificates');
      });
    });
  }

  function registerClearBrowsingDataTests() {
    suite('ClearBrowsingData', function() {
      /** @type {settings.TestClearBrowsingDataBrowserProxy} */
      var testBrowserProxy;

      /** @type {SettingsClearBrowsingDataDialogElement} */
      var element;

      setup(function() {
        testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
        settings.ClearBrowsingDataBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        element = document.createElement('settings-clear-browsing-data-dialog');
        document.body.appendChild(element);
      });

      teardown(function() { element.remove(); });

      test('ClearBrowsingDataTap', function() {
        assertTrue(element.$.dialog.opened);

        var cancelButton = element.$$('.cancel-button');
        assertTrue(!!cancelButton);
        var actionButton = element.$$('.action-button');
        assertTrue(!!actionButton);
        var spinner = element.$$('paper-spinner');
        assertTrue(!!spinner);

        assertFalse(cancelButton.disabled);
        assertFalse(actionButton.disabled);
        assertFalse(spinner.active);

        var promiseResolver = new PromiseResolver();
        testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
        MockInteractions.tap(actionButton);

        return testBrowserProxy.whenCalled('clearBrowsingData').then(
            function() {
              assertTrue(element.$.dialog.opened);
              assertTrue(cancelButton.disabled);
              assertTrue(actionButton.disabled);
              assertTrue(spinner.active);

              // Simulate signal from browser indicating that clearing has
              // completed.
              promiseResolver.resolve();
              // Yields to the message loop to allow the callback chain of the
              // Promise that was just resolved to execute before the
              // assertions.
            }).then(function() {
              assertFalse(element.$.dialog.opened);
              assertFalse(cancelButton.disabled);
              assertFalse(actionButton.disabled);
              assertFalse(spinner.active);
              assertFalse(!!element.$$('#notice'));
            });
      });

      test('showHistoryDeletionDialog', function() {
        assertTrue(element.$.dialog.opened);
        var actionButton = element.$$('.action-button');
        assertTrue(!!actionButton);

        var promiseResolver = new PromiseResolver();
        testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
        MockInteractions.tap(actionButton);

        return testBrowserProxy.whenCalled('clearBrowsingData').then(
            function() {
              // Passing showNotice = true should trigger the notice about other
              // forms of browsing history to open, and the dialog to stay open.
              promiseResolver.resolve(true /* showNotice */);

              // Yields to the message loop to allow the callback chain of the
              // Promise that was just resolved to execute before the
              // assertions.
            }).then(function() {
              Polymer.dom.flush();
              var notice = element.$$('#notice');
              assertTrue(!!notice);
              var noticeActionButton = notice.$$('.action-button');
              assertTrue(!!noticeActionButton);

              assertTrue(element.$.dialog.opened);
              assertTrue(notice.$.dialog.opened);

              MockInteractions.tap(noticeActionButton);

              // Tapping the action button will close the notice. Move to the
              // end of the message loop to allow the closing event to propagate
              // to the parent dialog. The parent dialog should subsequently
              // close as well.
              setTimeout(function() {
                var notice = element.$$('#notice');
                assertFalse(!!notice);
                assertFalse(element.$.dialog.opened);
              }, 0);
            });
      });

      test('Counters', function() {
        assertTrue(element.$.dialog.opened);

        // Initialize the browsing history pref, which should belong to the
        // first checkbox in the dialog.
        element.set('prefs', {
          browser: {
            clear_data: {
              browsing_history: {
                key: 'browser.clear_data.browsing_history',
                type: chrome.settingsPrivate.PrefType.BOOLEAN,
                value: true,
              }
            }
          }
        });
        var checkbox = element.$$('settings-checkbox');
        assertEquals('browser.clear_data.browsing_history', checkbox.pref.key);

        // Simulate a browsing data counter result for history. This checkbox's
        // sublabel should be updated.
        cr.webUIListenerCallback(
            'update-counter-text', checkbox.pref.key, 'result');
        assertEquals('result', checkbox.subLabel);

        // Unchecking the checkbox will hide its sublabel.
        var subLabelStyle = window.getComputedStyle(checkbox.$$('.secondary'));
        assertNotEquals('none', subLabelStyle.display);
        checkbox.checked = false;
        assertEquals('none', subLabelStyle.display);
      });
    });
  }

  return {
    registerTests: function() {
      if (cr.isMac || cr.isWin)
        registerNativeCertificateManagerTests();

      registerClearBrowsingDataTests();
    },
  };
});
