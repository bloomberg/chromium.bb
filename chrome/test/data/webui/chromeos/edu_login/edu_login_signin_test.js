// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/edu_login_signin.js';

import {EduAccountLoginBrowserProxyImpl} from 'chrome://chrome-signin/browser_proxy.js';
import {AuthMode, AuthParams} from 'chrome://chrome-signin/gaia_auth_host/authenticator.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TestEduAccountLoginBrowserProxy} from './edu_login_test_util.js';

window.edu_login_signin_tests = {};
edu_login_signin_tests.suiteName = 'EduLoginSigninTest';

/** @enum {string} */
edu_login_signin_tests.TestNames = {
  Init: 'Initial state',
  WebUICallbacks: 'WebUI callbacks test',
  AuthExtHostCallbacks: 'AuthExtHost callbacks test',
};

const fakeLoginParams = {
  reAuthProofToken: 'test-rapt',
  parentObfuscatedGaiaId: 'test-parent-gaia',
};

suite(edu_login_signin_tests.suiteName, function() {
  let signinComponent;
  let testBrowserProxy;
  let testAuthenticator;

  class TestAuthenticator extends EventTarget {
    constructor() {
      super();
      /** @type {AuthMode} */
      this.authMode = null;
      /** @type {AuthParams} */
      this.data = null;
      /** @type {Number} */
      this.loadCalls = 0;
      /** @type {Number} */
      this.resetStatesCalls = 0;
    }

    /**
     * @param {AuthMode} authMode Authorization mode.
     * @param {AuthParams} data Parameters for the authorization flow.
     */
    load(authMode, data) {
      this.loadCalls++;
      this.authMode = authMode;
      this.data = data;
    }

    resetStates() {
      this.resetStatesCalls++;
    }
  }

  setup(function() {
    testBrowserProxy = new TestEduAccountLoginBrowserProxy();
    EduAccountLoginBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    signinComponent = document.createElement('edu-login-signin');
    signinComponent.loginParams = fakeLoginParams;
    document.body.appendChild(signinComponent);
    testAuthenticator = new TestAuthenticator();
    signinComponent.setAuthExtHostForTest(testAuthenticator);
    flush();
  });

  test(assert(edu_login_signin_tests.TestNames.Init), function() {
    assertTrue(signinComponent.$$('.spinner').active);
    assertTrue(signinComponent.$$('#signinFrame').hidden);
    assertEquals(1, testBrowserProxy.getCallCount('loginInitialize'));
  });

  test(assert(edu_login_signin_tests.TestNames.WebUICallbacks), function() {
    let goBackCalls = 0;
    signinComponent.addEventListener('go-back', function() {
      goBackCalls++;
    });
    webUIListenerCallback('navigate-back-in-webview');
    expectEquals(1, goBackCalls);
    assertEquals(1, testAuthenticator.resetStatesCalls);

    const fakeAuthExtensionData = {
      hl: 'hl',
      gaiaUrl: 'gaiaUrl',
      authMode: 1,
      email: 'example@gmail.com',
    };
    webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
    assertEquals(1, testAuthenticator.loadCalls);
    assertEquals(fakeAuthExtensionData, testAuthenticator.data);
    assertEquals(fakeAuthExtensionData.authMode, testAuthenticator.authMode);

    webUIListenerCallback('close-dialog');
    assertEquals(1, testBrowserProxy.getCallCount('dialogClose'));
  });

  test(
      assert(edu_login_signin_tests.TestNames.AuthExtHostCallbacks),
      function() {
        assertTrue(signinComponent.$$('.spinner').active);
        testAuthenticator.dispatchEvent(new Event('ready'));
        assertEquals(1, testBrowserProxy.getCallCount('authExtensionReady'));
        assertFalse(signinComponent.$$('.spinner').active);

        const fakeUrl = 'www.google.com/fake';
        testAuthenticator.dispatchEvent(
            new CustomEvent('resize', {detail: fakeUrl}));
        testBrowserProxy.whenCalled('switchToFullTab').then(function(result) {
          assertEquals(fakeUrl, result);
        });

        const fakeCredentials = {email: 'example@gmail.com'};
        testAuthenticator.dispatchEvent(
            new CustomEvent('authCompleted', {detail: fakeCredentials}));
        testBrowserProxy.whenCalled('completeLogin').then(function(result) {
          assertTrue(signinComponent.$$('.spinner').active);
          assertEquals(fakeCredentials, result[0]);
          assertDeepEquals(fakeLoginParams, result[1]);
        });
      });
});
