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

  suite('PrivacyPage', function() {
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
});
