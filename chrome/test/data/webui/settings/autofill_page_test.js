// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_autofill_page', function() {
  /** @implements {settings.AutofillBrowserProxy} */
  class TestAutofillBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'openURL',
      ]);
    }

    /** @override */
    openURL(url) {
      this.methodCalled('openURL', url);
    }
  }

  suite('PasswordsUITest', function() {
    /** @type {SettingsAutofillPageElement} */
    let autofillPage = null;
    /** @type {settings.AutofillBrowserProxy} */
    let browserProxy = null;

    suiteSetup(function() {
      // Forces navigation to Google Password Manager to be off by default.
      loadTimeData.overrideValues({
        navigateToGooglePasswordManager: false,
      });
    });

    setup(function() {
      browserProxy = new TestAutofillBrowserProxy();
      settings.AutofillBrowserProxyImpl.instance_ = browserProxy;

      PolymerTest.clearBody();
      autofillPage = document.createElement('settings-autofill-page');
      document.body.appendChild(autofillPage);

      Polymer.dom.flush();
    });

    teardown(function() {
      autofillPage.remove();
    });

    test('Google Password Manager Off', function() {
      assertTrue(!!autofillPage.$$('#passwordManagerButton'));
      autofillPage.$$('#passwordManagerButton').click();
      Polymer.dom.flush();

      assertEquals(settings.getCurrentRoute(), settings.routes.PASSWORDS);
    });

    test('Google Password Manager On', function() {
      // Hardcode this value so that the test is independent of the production
      // implementation that might include additional query parameters.
      const googlePasswordManagerUrl = 'https://passwords.google.com';

      loadTimeData.overrideValues({
        navigateToGooglePasswordManager: true,
        googlePasswordManagerUrl: googlePasswordManagerUrl,
      });

      assertTrue(!!autofillPage.$$('#passwordManagerButton'));
      autofillPage.$$('#passwordManagerButton').click();
      Polymer.dom.flush();

      return browserProxy.whenCalled('openURL').then(url => {
        assertEquals(googlePasswordManagerUrl, url);
      });
    });
  });
});
