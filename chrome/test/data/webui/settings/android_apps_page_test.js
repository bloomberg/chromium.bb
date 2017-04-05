// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.AndroidAppsBrowserProxy}
 * @extends {settings.TestBrowserProxy}
 */
function TestAndroidAppsBrowserProxy() {
  settings.TestBrowserProxy.call(this, [
    'requestAndroidAppsInfo',
    'showAndroidAppsSettings',
  ]);
}

TestAndroidAppsBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  /** @override */
  requestAndroidAppsInfo: function() {
    this.methodCalled('requestAndroidAppsInfo');
    cr.webUIListenerCallback('android-apps-info-update', {appReady: false});
  },

  /** override */
  showAndroidAppsSettings: function(keyboardAction) {
    this.methodCalled('showAndroidAppsSettings');
  },

  setAppReady: function(ready) {
    // We need to make sure to pass a new object here, otherwise the property
    // change event may not get fired in the listener.
    cr.webUIListenerCallback('android-apps-info-update', {appReady: ready});
  },
};

/** @type {?SettingsAndroidAppsPageElement} */
var androidAppsPage = null;

/** @type {?TestAndroidAppsBrowserProxy} */
var androidAppsBrowserProxy = null;

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

  suite('Main Page', function() {
    setup(function() {
      androidAppsPage.prefs = {arc: {enabled: {value: false}}};
      Polymer.dom.flush();

      return androidAppsBrowserProxy.whenCalled('requestAndroidAppsInfo')
          .then(function() {
            androidAppsBrowserProxy.setAppReady(false);
          });
    });

    test('Enable', function() {
      var button = androidAppsPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!androidAppsPage.$$('.subpage-arrow'));

      MockInteractions.tap(button);
      Polymer.dom.flush();
      assertTrue(androidAppsPage.prefs.arc.enabled.value);

      androidAppsBrowserProxy.setAppReady(true);
      Polymer.dom.flush();
      assertTrue(!!androidAppsPage.$$('.subpage-arrow'));
    });
  });

  suite('SubPage', function() {
    var subpage;

    setup(function() {
      androidAppsPage.prefs = {arc: {enabled: {value: true}}};
      return androidAppsBrowserProxy.whenCalled('requestAndroidAppsInfo')
          .then(function() {
            androidAppsBrowserProxy.setAppReady(true);
            MockInteractions.tap(androidAppsPage.$$('#android-apps'));
            Polymer.dom.flush();
            subpage = androidAppsPage.$$('settings-android-apps-subpage');
            assertTrue(!!subpage);
          });
    });

    test('Sanity', function() {
      assertTrue(!!subpage.$$('#manageApps'));
      assertTrue(!!subpage.$$('#remove'));
    });

    test('Disable', function() {
      var dialog = subpage.$$('#confirmDisableDialog');
      assertTrue(!!dialog);
      assertFalse(dialog.open);

      var remove = subpage.$$('#remove');
      assertTrue(!!remove);
      MockInteractions.tap(remove);

      Polymer.dom.flush();
      assertTrue(dialog.open);
    });
  });

  suite('Enforced', function() {
    var subpage;

    setup(function() {
      androidAppsPage.prefs = {
        arc: {
          enabled: {
            value: true,
            enforcement: chrome.settingsPrivate.Enforcement.ENFORCED
          }
        }
      };
      return androidAppsBrowserProxy.whenCalled('requestAndroidAppsInfo')
          .then(function() {
            androidAppsBrowserProxy.setAppReady(true);
            MockInteractions.tap(androidAppsPage.$$('#android-apps'));
            Polymer.dom.flush();
            subpage = androidAppsPage.$$('settings-android-apps-subpage');
            assertTrue(!!subpage);
          });
    });

    test('Sanity', function() {
      Polymer.dom.flush();
      assertTrue(!!subpage.$$('#manageApps'));
      assertFalse(!!subpage.$$('#remove'));
    });
  });
});
