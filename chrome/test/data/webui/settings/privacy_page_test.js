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
    settings.TestBrowserProxy.call(this, ['clearBrowsingData']);
  }

  TestClearBrowsingDataBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    clearBrowsingData: function() {
      this.methodCalled('clearBrowsingData');
      return Promise.resolve();
    },
  };

  function registerNativeCertificateManagerTests() {
    suite('NativeCertificateManager', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      var testBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      var page;

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
        element.open();
        assertTrue(element.$.dialog.opened);
        var clearBrowsingDataButton = element.$.clearBrowsingData;
        assertTrue(!!clearBrowsingDataButton);

        MockInteractions.tap(clearBrowsingDataButton);
        return testBrowserProxy.whenCalled('clearBrowsingData').then(
            function() {
              assertFalse(element.$.dialog.opened);
            });
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
