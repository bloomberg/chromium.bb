// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('onboarding_signin_view_test', function() {
  suite('SigninViewTest', function() {

    /** @type {SigninViewElement} */
    let testElement;

    /** @type {welcome.WelcomeBrowserProxy} */
    let testWelcomeBrowserProxy;

    /** @type {nux.NuxEmailProxy} */
    let testEmailBrowserProxy;

    setup(function() {
      testWelcomeBrowserProxy = new TestWelcomeBrowserProxy();
      welcome.WelcomeBrowserProxyImpl.instance_ = testWelcomeBrowserProxy;

      testEmailBrowserProxy = new TestNuxEmailProxy();
      nux.NuxEmailProxyImpl.instance_ = testEmailBrowserProxy;

      PolymerTest.clearBody();
      testElement = document.createElement('signin-view');
      document.body.appendChild(testElement);
    });

    teardown(function() {
      testElement.remove();
    });

    test('sign-in button with no email provider selected', function() {
      const signinButton = testElement.$$('paper-button');
      assertTrue(!!signinButton);

      signinButton.click();
      return testEmailBrowserProxy.whenCalled('getSavedProvider')
          .then(() => {
            return testWelcomeBrowserProxy.whenCalled('handleActivateSignIn');
          })
          .then(redirectUrl => {
            assertEquals(redirectUrl, null);
          });
    });

    test('sign-in button with a email provider selected', function() {
      const signinButton = testElement.$$('paper-button');
      assertTrue(!!signinButton);

      testEmailBrowserProxy.setSavedProvider(3);

      signinButton.click();
      return testEmailBrowserProxy.whenCalled('getSavedProvider')
          .then(() => {
            return testWelcomeBrowserProxy.whenCalled('handleActivateSignIn');
          })
          .then(redirectUrl => {
            assertEquals(
                redirectUrl, 'chrome://welcome/email-interstitial?provider=3');
          });
    });

    test('no-thanks button with no email provider selected', function() {
      const noThanksButton = testElement.$$('button');
      assertTrue(!!noThanksButton);

      noThanksButton.click();
      return Promise.all([
        testEmailBrowserProxy.whenCalled('getSavedProvider'),
        testWelcomeBrowserProxy.whenCalled('goToNewTabPage'),
      ]);
    });

    test('no-thanks button with an email provider selected', function() {
      const noThanksButton = testElement.$$('button');
      assertTrue(!!noThanksButton);

      testEmailBrowserProxy.setSavedProvider(4);

      noThanksButton.click();
      return testEmailBrowserProxy.whenCalled('getSavedProvider')
          .then(() => {
            return testWelcomeBrowserProxy.whenCalled('goToURL');
          })
          .then(url => {
            assertEquals(url, 'chrome://welcome/email-interstitial?provider=4');
          });
    });
  });
});
