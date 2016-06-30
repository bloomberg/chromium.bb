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
      'getProfileManagesSupervisedUsers',
    ]);

    this.fakeProfileInfo = {
      name: 'fakeName',
      iconUrl: 'http://fake-icon-url.com/',
    };
  };

  TestProfileInfoBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    getProfileInfo: function() {
      this.methodCalled('getProfileInfo');
      return Promise.resolve(this.fakeProfileInfo);
    },

    /** @override */
    getProfileManagesSupervisedUsers: function() {
      this.methodCalled('getProfileManagesSupervisedUsers');
      return Promise.resolve(false);
    }
  };

  /**
   * @constructor
   * @implements {settings.SyncBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestSyncBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'getSyncStatus',
      'signOut',
    ]);
  };

  TestSyncBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    getSyncStatus: function() {
      this.methodCalled('getSyncStatus');
      return Promise.resolve({
        signedIn: true,
      });
    },

    /** @override */
    signOut: function(deleteProfile) {
      this.methodCalled('signOut', deleteProfile);
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

      test('GetProfileManagesSupervisedUsers', function() {
        return browserProxy.whenCalled('getProfileManagesSupervisedUsers').then(
            function() {
              Polymer.dom.flush();
              assertFalse(!!peoplePage.$$('#manageSupervisedUsersContainer'));

              cr.webUIListenerCallback(
                'profile-manages-supervised-users-changed',
                true);

              Polymer.dom.flush();
              assertTrue(!!peoplePage.$$('#manageSupervisedUsersContainer'));
            });
      });
    });
  }

  function registerSyncStatusTests() {
    suite('SyncStatusTests', function() {
      var peoplePage = null;
      var browserProxy = null;

      suiteSetup(function() {
        // Force easy unlock off. Those have their own ChromeOS-only tests.
        loadTimeData.overrideValues({
          easyUnlockAllowed: false,
        });
      });

      setup(function() {
        browserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = browserProxy;

        PolymerTest.clearBody();
        peoplePage = document.createElement('settings-people-page');
        document.body.appendChild(peoplePage);
      });

      teardown(function() { peoplePage.remove(); });

      test('GetProfileInfo', function() {
        return browserProxy.whenCalled('getSyncStatus').then(function() {
          Polymer.dom.flush();
          var disconnectButton = peoplePage.$$('#disconnectButton');
          assertTrue(!!disconnectButton);

          MockInteractions.tap(disconnectButton);
          Polymer.dom.flush();

          assertTrue(peoplePage.$.disconnectDialog.opened);
          assertFalse(peoplePage.$.deleteProfile.hidden);

          var disconnectConfirm = peoplePage.$.disconnectConfirm;
          assertTrue(!!disconnectConfirm);
          assertFalse(disconnectConfirm.hidden);
          MockInteractions.tap(disconnectConfirm);

          return browserProxy.whenCalled('signOut').then(
              function(deleteProfile) {
                Polymer.dom.flush();

                assertFalse(deleteProfile);

                cr.webUIListenerCallback('sync-status-changed', {
                  signedIn: true,
                  domain: 'example.com',
                });
                Polymer.dom.flush();

                assertFalse(peoplePage.$.disconnectDialog.opened);
                MockInteractions.tap(disconnectButton);
                Polymer.dom.flush();

                assertTrue(peoplePage.$.disconnectDialog.opened);
                assertTrue(peoplePage.$.deleteProfile.hidden);

                var disconnectManagedProfileConfirm =
                    peoplePage.$.disconnectManagedProfileConfirm;
                assertTrue(!!disconnectManagedProfileConfirm);
                assertFalse(disconnectManagedProfileConfirm.hidden);

                browserProxy.resetResolver('signOut');
                MockInteractions.tap(disconnectManagedProfileConfirm);

                return browserProxy.whenCalled('signOut').then(
                    function(deleteProfile) {
                      assertTrue(deleteProfile);
                    });
              });
        });
      });
    });
  }

  return {
    registerTests: function() {
      registerProfileInfoTests();
      if (!cr.isChromeOS)
        registerSyncStatusTests();
    },
  };
});
