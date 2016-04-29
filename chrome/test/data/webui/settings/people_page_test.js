// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page', function() {
  /**
   * @constructor
   * @implements {settings.ProfileInfoBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestProfileInfoBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'getProfileInfo',
    ]);
  };

  TestProfileInfoBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    fakeProfileInfo: {
      name: 'fakeName',
      iconUrl: 'http://fake-icon-url.com/',
    },

    /** @override */
    getProfileInfo: function() {
      this.methodCalled('getProfileInfo');
      return Promise.resolve(this.fakeProfileInfo);
    },
  };

  function registerProfileInfoTests() {
    suite('ProfileInfoTests', function() {
      var peoplePage = null;
      var browserProxy = null;

      suiteSetup(function() {
        // Force easy unlock off. Those have their own ChromeOS-only tests.
        loadTimeData.overrideValues({
          easyUnlockAllowed: false,
        });
      });

      setup(function() {
        browserProxy = new TestProfileInfoBrowserProxy();
        settings.ProfileInfoBrowserProxyImpl.instance_ = browserProxy;

        PolymerTest.clearBody();
        peoplePage = document.createElement('settings-people-page');
        document.body.appendChild(peoplePage);
      });

      teardown(function() { peoplePage.remove(); });

      test('GetProfileInfo', function() {
        return browserProxy.whenCalled('getProfileInfo').then(function() {
          Polymer.dom.flush();
          assertEquals(browserProxy.fakeProfileInfo.name,
                       peoplePage.$$('#profile-name').textContent.trim());
          assertEquals(browserProxy.fakeProfileInfo.iconUrl,
                       peoplePage.$$('#profile-icon').src);

          cr.webUIListenerCallback(
            'profile-info-changed',
            {name: 'pushedName', iconUrl: 'http://pushed-url/'});

          Polymer.dom.flush();
          assertEquals('pushedName',
                       peoplePage.$$('#profile-name').textContent.trim());
          assertEquals('http://pushed-url/',
                       peoplePage.$$('#profile-icon').src);
        });
      });
    });
  }

  return {
    registerTests: function() {
      registerProfileInfoTests();
    },
  };
});
