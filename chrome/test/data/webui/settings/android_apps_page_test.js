// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?SettingsAndroidAppsPageElement} */
var androidAppsPage = null;

/** @type {?TestAndroidAppsBrowserProxy} */
var androidAppsBrowserProxy = null;

var setAndroidAppsState = function(playStoreEnabled, settingsAppAvailable) {
  var appsInfo = {
    playStoreEnabled: playStoreEnabled,
    settingsAppAvailable: settingsAppAvailable,
  };
  androidAppsPage.androidAppsInfo = appsInfo;
  Polymer.dom.flush();
};

suite('AndroidAppsPageTests', function() {
  setup(function() {
    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    settings.AndroidAppsBrowserProxyImpl.instance_ = androidAppsBrowserProxy;
    PolymerTest.clearBody();
    androidAppsPage = document.createElement('settings-android-apps-page');
    document.body.appendChild(androidAppsPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    androidAppsPage.remove();
  });

  teardown(function() {
    androidAppsPage.remove();
  });

  suite('Main Page', function() {
    setup(function() {
      androidAppsPage.havePlayStoreApp = true;
      androidAppsPage.prefs = {arc: {enabled: {value: false}}};
      setAndroidAppsState(false, false);
    });

    test('Enable', function() {
      var button = androidAppsPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!androidAppsPage.$$('.subpage-arrow'));

      MockInteractions.tap(button);
      Polymer.dom.flush();
      assertTrue(androidAppsPage.prefs.arc.enabled.value);

      setAndroidAppsState(true, false);
      assertTrue(!!androidAppsPage.$$('.subpage-arrow'));
    });
  });

  suite('SubPage', function() {
    var subpage;

    /**
     * Returns a new promise that resolves after a window 'popstate' event.
     * @return {!Promise}
     */
    function whenPopState() {
      return new Promise(function(resolve) {
        window.addEventListener('popstate', function callback() {
          window.removeEventListener('popstate', callback);
          resolve();
        });
      });
    }

    setup(function() {
      androidAppsPage.havePlayStoreApp = true;
      androidAppsPage.prefs = {arc: {enabled: {value: true}}};
      setAndroidAppsState(true, false);
      settings.navigateTo(settings.routes.ANDROID_APPS);
      MockInteractions.tap(androidAppsPage.$$('#android-apps'));
      Polymer.dom.flush();
      subpage = androidAppsPage.$$('settings-android-apps-subpage');
      assertTrue(!!subpage);
    });

    test('Sanity', function() {
      assertTrue(!!subpage.$$('#remove'));
      assertTrue(!subpage.$$('settings-android-settings-element'));
    });

    test('ManageAppsUpdate', function() {
      assertTrue(!subpage.$$('settings-android-settings-element'));
      setAndroidAppsState(true, true);
      assertTrue(!!subpage.$$('settings-android-settings-element'));
      assertTrue(!!subpage.$$('settings-android-settings-element').
          $$('#manageApps'));
      setAndroidAppsState(true, false);
      assertTrue(!subpage.$$('settings-android-settings-element'));
    });

    test('ManageAppsOpenRequest', function() {
      setAndroidAppsState(true, true);
      var button = subpage.$$('settings-android-settings-element').
          $$('#manageApps');
      assertTrue(!!button);
      var promise = androidAppsBrowserProxy.whenCalled(
          'showAndroidAppsSettings');
      // MockInteractions.tap does not work here due style is not updated.
      button.click();
      Polymer.dom.flush();
      return promise;
    });

    test('Disable', function() {
      var dialog = subpage.$$('#confirmDisableDialog');
      assertTrue(!!dialog);
      assertFalse(dialog.open);

      var remove = subpage.$$('#remove');
      assertTrue(!!remove);

      subpage.onRemoveTap_();
      Polymer.dom.flush();
      assertTrue(dialog.open);
      dialog.close();
    });

    test('HideOnDisable', function() {
      assertEquals(
          settings.getCurrentRoute(), settings.routes.ANDROID_APPS_DETAILS);
      setAndroidAppsState(false, false);
      return whenPopState().then(function() {
        assertEquals(settings.getCurrentRoute(), settings.routes.ANDROID_APPS);
      });
    });
  });

  suite('Enforced', function() {
    var subpage;

    setup(function() {
      androidAppsPage.havePlayStoreApp = true;
      androidAppsPage.prefs = {
        arc: {
          enabled: {
            value: true,
            enforcement: chrome.settingsPrivate.Enforcement.ENFORCED
          }
        }
      };
      setAndroidAppsState(true, true);
      MockInteractions.tap(androidAppsPage.$$('#android-apps'));
      Polymer.dom.flush();
      subpage = androidAppsPage.$$('settings-android-apps-subpage');
      assertTrue(!!subpage);
    });

    test('Sanity', function() {
      Polymer.dom.flush();
      assertFalse(!!subpage.$$('#remove'));
      assertTrue(!!subpage.$$('settings-android-settings-element'));
      assertTrue(!!subpage.$$('settings-android-settings-element').
          $$('#manageApps'));
    });
  });

  suite('NoPlayStore', function() {
    setup(function() {
      androidAppsPage.havePlayStoreApp = false;
      androidAppsPage.prefs = {arc: {enabled: {value: true}}};
      setAndroidAppsState(true, true);
    });

    test('Sanity', function() {
      assertTrue(!!androidAppsPage.$$('settings-android-settings-element'));
      assertTrue(!!androidAppsPage.$$('settings-android-settings-element').
          $$("#manageApps"));
    });

    test('ManageAppsOpenRequest', function() {
      var button = androidAppsPage.$$('settings-android-settings-element').
          $$('#manageApps');
      assertTrue(!!button);
      var promise = androidAppsBrowserProxy.whenCalled(
          'showAndroidAppsSettings');
      // MockInteractions.tap does not work here due style is not updated.
      button.click();
      Polymer.dom.flush();
      return promise;
    });
  });

});
