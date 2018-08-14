// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_privacy_page', function() {
  /** @implements {settings.ClearBrowsingDataBrowserProxy} */
  class TestClearBrowsingDataBrowserProxy extends TestBrowserProxy {
    constructor() {
      super(['initialize', 'clearBrowsingData']);

      /**
       * The promise to return from |clearBrowsingData|.
       * Allows testing code to test what happens after the call is made, and
       * before the browser responds.
       * @private {?Promise}
       */
      this.clearBrowsingDataPromise_ = null;
    }

    /** @param {!Promise} promise */
    setClearBrowsingDataPromise(promise) {
      this.clearBrowsingDataPromise_ = promise;
    }

    /** @override */
    clearBrowsingData(dataTypes, timePeriod) {
      this.methodCalled('clearBrowsingData', [dataTypes, timePeriod]);
      cr.webUIListenerCallback('browsing-data-removing', true);
      return this.clearBrowsingDataPromise_ !== null ?
          this.clearBrowsingDataPromise_ :
          Promise.resolve();
    }

    /** @override */
    initialize() {
      this.methodCalled('initialize');
      return Promise.resolve(false);
    }
  }

  function getClearBrowsingDataPrefs() {
    return {
      browser: {
        clear_data: {
          time_period: {
            key: 'browser.clear_data.time_period',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
          time_period_basic: {
            key: 'browser.clear_data.time_period_basic',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
          browsing_history: {
            key: 'browser.clear_data.browsing_history',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          cookies: {
            key: 'browser.clear_data.cookies',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          cookies_basic: {
            key: 'browser.clear_data.cookies_basic',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          cache_basic: {
            key: 'browser.clear_data.cache_basic',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
        last_clear_browsing_data_tab: {
          key: 'browser.last_clear_browsing_data_tab',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
      }
    };
  }

  function registerNativeCertificateManagerTests() {
    suite('NativeCertificateManager', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      let page;

      setup(function() {
        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
      });

      teardown(function() {
        page.remove();
      });

      test('NativeCertificateManager', function() {
        page.$$('#manageCertificates').click();
        return testBrowserProxy.whenCalled('showManageSSLCertificates');
      });
    });
  }

  function registerPrivacyPageTests() {
    suite('PrivacyPage', function() {
      /** @type {SettingsPrivacyPageElement} */
      let page;

      setup(function() {
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
      });

      teardown(function() {
        page.remove();
      });

      test('showClearBrowsingDataDialog', function() {
        assertFalse(!!page.$$('settings-clear-browsing-data-dialog'));
        page.$$('#clearBrowsingData').click();
        Polymer.dom.flush();

        const dialog = page.$$('settings-clear-browsing-data-dialog');
        assertTrue(!!dialog);

        // Ensure that the dialog is fully opened before returning from this
        // test, otherwise asynchronous code run in attached() can cause flaky
        // errors.
        return test_util.whenAttributeIs(
            dialog.$$('#clearBrowsingDataDialog'), 'open', '');
      });
    });
  }

  function registerClearBrowsingDataTests() {
    suite('ClearBrowsingData', function() {
      /** @type {settings.TestClearBrowsingDataBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsClearBrowsingDataDialogElement} */
      let element;

      setup(function() {
        testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
        settings.ClearBrowsingDataBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        element = document.createElement('settings-clear-browsing-data-dialog');
        element.set('prefs', getClearBrowsingDataPrefs());
        document.body.appendChild(element);
        return testBrowserProxy.whenCalled('initialize');
      });

      teardown(function() {
        element.remove();
      });

      test('ClearBrowsingDataTap', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);

        const cancelButton = element.$$('.cancel-button');
        assertTrue(!!cancelButton);
        const actionButton = element.$$('.action-button');
        assertTrue(!!actionButton);
        const spinner = element.$$('paper-spinner-lite');
        assertTrue(!!spinner);

        // Select a datatype for deletion to enable the clear button.
        const cookieCheckbox = element.$$('#cookiesCheckboxBasic');
        assertTrue(!!cookieCheckbox);
        cookieCheckbox.$.checkbox.click();

        assertFalse(cancelButton.disabled);
        assertFalse(actionButton.disabled);
        assertFalse(spinner.active);

        const promiseResolver = new PromiseResolver();
        testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
        actionButton.click();

        return testBrowserProxy.whenCalled('clearBrowsingData')
            .then(function(args) {
              const dataTypes = args[0];
              const timePeriod = args[1];
              assertEquals(1, dataTypes.length);
              assertEquals('browser.clear_data.cookies_basic', dataTypes[0]);
              assertTrue(element.$$('#clearBrowsingDataDialog').open);
              assertTrue(cancelButton.disabled);
              assertTrue(actionButton.disabled);
              assertTrue(spinner.active);

              // Simulate signal from browser indicating that clearing has
              // completed.
              cr.webUIListenerCallback('browsing-data-removing', false);
              promiseResolver.resolve();
              // Yields to the message loop to allow the callback chain of the
              // Promise that was just resolved to execute before the
              // assertions.
            })
            .then(function() {
              assertFalse(element.$$('#clearBrowsingDataDialog').open);
              assertFalse(cancelButton.disabled);
              assertFalse(actionButton.disabled);
              assertFalse(spinner.active);
              assertFalse(!!element.$$('#notice'));
            });
      });

      test('ClearBrowsingDataClearButton', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);

        const actionButton = element.$$('.action-button');
        assertTrue(!!actionButton);
        const cookieCheckboxBasic = element.$$('#cookiesCheckboxBasic');
        assertTrue(!!cookieCheckboxBasic);
        const basicTab = element.$$('#basicTabTitle');
        assertTrue(!!basicTab);
        const advancedTab = element.$$('#advancedTabTitle');
        assertTrue(!!advancedTab);
        // Initially the button is disabled because all checkboxes are off.
        assertTrue(actionButton.disabled);
        // The button gets enabled if any checkbox is selected.
        cookieCheckboxBasic.$.checkbox.click();
        assertTrue(cookieCheckboxBasic.checked);
        assertFalse(actionButton.disabled);
        // Switching to advanced disables the button.
        advancedTab.click();
        assertTrue(actionButton.disabled);
        // Switching back enables it again.
        basicTab.click();
        assertFalse(actionButton.disabled);
      });

      test('showHistoryDeletionDialog', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);
        const actionButton = element.$$('.action-button');
        assertTrue(!!actionButton);

        // Select a datatype for deletion to enable the clear button.
        const cookieCheckbox = element.$$('#cookiesCheckboxBasic');
        assertTrue(!!cookieCheckbox);
        cookieCheckbox.$.checkbox.click();
        assertFalse(actionButton.disabled);

        const promiseResolver = new PromiseResolver();
        testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
        actionButton.click();

        return testBrowserProxy.whenCalled('clearBrowsingData')
            .then(function() {
              // Passing showNotice = true should trigger the notice about other
              // forms of browsing history to open, and the dialog to stay open.
              promiseResolver.resolve(true /* showNotice */);

              // Yields to the message loop to allow the callback chain of the
              // Promise that was just resolved to execute before the
              // assertions.
            })
            .then(function() {
              Polymer.dom.flush();
              const notice = element.$$('#notice');
              assertTrue(!!notice);
              const noticeActionButton = notice.$$('.action-button');
              assertTrue(!!noticeActionButton);

              assertTrue(element.$$('#clearBrowsingDataDialog').open);
              assertTrue(notice.$$('#dialog').open);

              noticeActionButton.click();

              return new Promise(function(resolve, reject) {
                // Tapping the action button will close the notice. Move to the
                // end of the message loop to allow the closing event to
                // propagate to the parent dialog. The parent dialog should
                // subsequently close as well.
                setTimeout(function() {
                  const notice = element.$$('#notice');
                  assertFalse(!!notice);
                  assertFalse(element.$$('#clearBrowsingDataDialog').open);
                  resolve();
                }, 0);
              });
            });
      });

      test('Counters', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);

        const checkbox = element.$$('#cacheCheckboxBasic');
        assertEquals('browser.clear_data.cache_basic', checkbox.pref.key);

        // Simulate a browsing data counter result for history. This checkbox's
        // sublabel should be updated.
        cr.webUIListenerCallback(
            'update-counter-text', checkbox.pref.key, 'result');
        assertEquals('result', checkbox.subLabel);
      });

      test('history rows are hidden for supervised users', function() {
        assertFalse(loadTimeData.getBoolean('isSupervised'));
        assertFalse(element.$$('#browsingCheckbox').hidden);
        assertFalse(element.$$('#browsingCheckboxBasic').hidden);
        assertFalse(element.$$('#downloadCheckbox').hidden);

        element.remove();
        testBrowserProxy.reset();
        loadTimeData.overrideValues({isSupervised: true});

        element = document.createElement('settings-clear-browsing-data-dialog');
        document.body.appendChild(element);
        Polymer.dom.flush();

        return testBrowserProxy.whenCalled('initialize').then(function() {
          assertTrue(element.$$('#browsingCheckbox').hidden);
          assertTrue(element.$$('#browsingCheckboxBasic').hidden);
          assertTrue(element.$$('#downloadCheckbox').hidden);
        });
      });
    });
  }

  function registerPrivacyPageSoundTests() {
    suite('PrivacyPageSound', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      let page;

      function flushAsync() {
        Polymer.dom.flush();
        return new Promise(resolve => {
          page.async(resolve);
        });
      }

      function getToggleElement() {
        return page.$$('settings-animated-pages')
            .queryEffectiveChildren('settings-subpage')
            .queryEffectiveChildren('#block-autoplay-setting');
      }

      setup(() => {
        loadTimeData.overrideValues({
          enableSoundContentSetting: true,
          enableBlockAutoplayContentSetting: true
        });

        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();

        settings.router.navigateTo(settings.routes.SITE_SETTINGS_SOUND);
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
        return flushAsync();
      });

      teardown(() => {
        page.remove();
      });

      test('UpdateStatus', () => {
        assertTrue(getToggleElement().hasAttribute('disabled'));
        assertFalse(getToggleElement().hasAttribute('checked'));

        cr.webUIListenerCallback(
            'onBlockAutoplayStatusChanged',
            {pref: {value: true}, enabled: true});

        return flushAsync().then(() => {
          // Check that we are on and enabled.
          assertFalse(getToggleElement().hasAttribute('disabled'));
          assertTrue(getToggleElement().hasAttribute('checked'));

          // Toggle the pref off.
          cr.webUIListenerCallback(
              'onBlockAutoplayStatusChanged',
              {pref: {value: false}, enabled: true});

          return flushAsync().then(() => {
            // Check that we are off and enabled.
            assertFalse(getToggleElement().hasAttribute('disabled'));
            assertFalse(getToggleElement().hasAttribute('checked'));

            // Disable the autoplay status toggle.
            cr.webUIListenerCallback(
                'onBlockAutoplayStatusChanged',
                {pref: {value: false}, enabled: false});

            return flushAsync().then(() => {
              // Check that we are off and disabled.
              assertTrue(getToggleElement().hasAttribute('disabled'));
              assertFalse(getToggleElement().hasAttribute('checked'));
            });
          });
        });
      });

      test('Hidden', () => {
        assertTrue(
            loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
        assertFalse(getToggleElement().hidden);

        loadTimeData.overrideValues({enableBlockAutoplayContentSetting: false});

        page.remove();
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);

        return flushAsync().then(() => {
          assertFalse(
              loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
          assertTrue(getToggleElement().hidden);
        });
      });

      test('Click', () => {
        assertTrue(getToggleElement().hasAttribute('disabled'));
        assertFalse(getToggleElement().hasAttribute('checked'));

        cr.webUIListenerCallback(
            'onBlockAutoplayStatusChanged',
            {pref: {value: true}, enabled: true});

        return flushAsync().then(() => {
          // Check that we are on and enabled.
          assertFalse(getToggleElement().hasAttribute('disabled'));
          assertTrue(getToggleElement().hasAttribute('checked'));

          // Click on the toggle and wait for the proxy to be called.
          getToggleElement().click();
          return testBrowserProxy.whenCalled('setBlockAutoplayEnabled')
              .then((enabled) => {
                assertFalse(enabled);
              });
        });
      });
    });
  }

  if (cr.isMac || cr.isWindows)
    registerNativeCertificateManagerTests();

  registerClearBrowsingDataTests();
  registerPrivacyPageTests();
  registerPrivacyPageSoundTests();
});
