// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('onboarding_welcome_app_test', function() {
  suite('WelcomeAppTest', function() {

    /** @type {WelcomeAppElement} */
    let testElement;

    /** @type {welcome.WelcomeBrowserProxy} */
    let testWelcomeBrowserProxy;

    /** @type {nux.NuxSetAsDefaultProxy} */
    let testSetAsDefaultProxy;

    function resetTestElement() {
      PolymerTest.clearBody();
      welcome.navigateTo(welcome.Routes.LANDING, 'landing');
      testElement = document.createElement('welcome-app');
      document.body.appendChild(testElement);
    }

    function simulateCanSetDefault() {
      testSetAsDefaultProxy.setDefaultStatus({
        isDefault: false,
        canBeDefault: true,
        isDisabledByPolicy: false,
        isUnknownError: false,
      });

      resetTestElement();
    }

    function simulateCannotSetDefault() {
      testSetAsDefaultProxy.setDefaultStatus({
        isDefault: true,
        canBeDefault: true,
        isDisabledByPolicy: false,
        isUnknownError: false,
      });

      resetTestElement();
    }

    setup(function() {
      testWelcomeBrowserProxy = new TestWelcomeBrowserProxy();
      welcome.WelcomeBrowserProxyImpl.instance_ = testWelcomeBrowserProxy;

      testSetAsDefaultProxy = new TestNuxSetAsDefaultProxy();
      nux.NuxSetAsDefaultProxyImpl.instance_ = testSetAsDefaultProxy;

      resetTestElement();
    });

    teardown(function() {
      testElement.remove();
    });

    test('shows landing page by default', function() {
      assertEquals(
          testElement.shadowRoot.querySelectorAll('[slot=view]').length, 1);
      assertTrue(!!testElement.$$('landing-view'));
      assertTrue(testElement.$$('landing-view').classList.contains('active'));
    });

    test('new user route (can set default)', function() {
      simulateCanSetDefault();
      welcome.navigateTo(welcome.Routes.NEW_USER, 1);
      return test_util.waitForRender(testElement).then(() => {
        const views = testElement.shadowRoot.querySelectorAll('[slot=view]');
        assertEquals(views.length, 5);
        assertEquals(views[0].tagName, 'LANDING-VIEW');
        assertEquals(views[1].tagName, 'NUX-EMAIL');
        assertEquals(views[2].tagName, 'NUX-GOOGLE-APPS');
        assertEquals(views[3].tagName, 'NUX-SET-AS-DEFAULT');
        assertEquals(views[4].tagName, 'SIGNIN-VIEW');
      });
    });

    test('new user route (canot set default)', function() {
      simulateCannotSetDefault();
      welcome.navigateTo(welcome.Routes.NEW_USER, 1);
      return test_util.waitForRender(testElement).then(() => {
        const views = testElement.shadowRoot.querySelectorAll('[slot=view]');
        assertEquals(views.length, 4);
        assertEquals(views[0].tagName, 'LANDING-VIEW');
        assertEquals(views[1].tagName, 'NUX-EMAIL');
        assertEquals(views[2].tagName, 'NUX-GOOGLE-APPS');
        assertEquals(views[3].tagName, 'SIGNIN-VIEW');
      });
    });

    test('returning user route (can set default)', function() {
      simulateCanSetDefault();
      welcome.navigateTo(welcome.Routes.RETURNING_USER, 1);
      return test_util.waitForRender(testElement).then(() => {
        const views = testElement.shadowRoot.querySelectorAll('[slot=view]');
        assertEquals(views.length, 2);
        assertEquals(views[0].tagName, 'LANDING-VIEW');
        assertEquals(views[1].tagName, 'NUX-SET-AS-DEFAULT');
      });
    });

    test('returning user route (cannot set default)', function() {
      simulateCannotSetDefault();
      welcome.navigateTo(welcome.Routes.RETURNING_USER, 1);

      // At this point, there should be no steps in the returning user, so
      // welcome_app should try to go to NTP.
      return testWelcomeBrowserProxy.whenCalled('goToNewTabPage');
    });
  });
});
