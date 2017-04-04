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

  /** @private {!AndroidAppsInfo} */
  this.androidAppsInfo_ = {appReady: false};
}

TestAndroidAppsBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  /** @override */
  requestAndroidAppsInfo: function() {
    this.methodCalled('requestAndroidAppsInfo');
  },

  /** override */
  showAndroidAppsSettings: function(keyboardAction) {
    this.methodCalled('showAndroidAppsSettings');
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
  });

  teardown(function() { androidAppsPage.remove(); });

  test('Enable', function() {
    return androidAppsBrowserProxy.whenCalled('requestAndroidAppsInfo')
        .then(function() {
          androidAppsPage.prefs = {
            arc: {
              enabled: {
                value: false,
              },
            },
          };
          cr.webUIListenerCallback(
              'android-apps-info-update', {appReady: false});
          Polymer.dom.flush();
          var control = androidAppsPage.$$('#enabled');
          assertTrue(!!control);
          assertFalse(control.disabled);
          assertFalse(control.checked);
          var managed = androidAppsPage.$$('#manageApps');
          assertTrue(!!managed);
          assertTrue(managed.hidden);

          MockInteractions.tap(control.$.control);
          Polymer.dom.flush();
          assertTrue(control.checked);
        });
  });

  test('Disable', function() {
    return androidAppsBrowserProxy.whenCalled('requestAndroidAppsInfo')
        .then(function() {
          androidAppsPage.prefs = {
            arc: {
              enabled: {
                value: true,
              },
            },
          };
          cr.webUIListenerCallback(
              'android-apps-info-update', {appReady: true});
          Polymer.dom.flush();
          var control = androidAppsPage.$$('#enabled');
          assertTrue(!!control);
          assertFalse(control.disabled);
          assertTrue(control.checked);
          var managed = androidAppsPage.$$('#manageApps');
          assertTrue(!!managed);
          assertFalse(managed.hidden);

          MockInteractions.tap(control.$.control);
          cr.webUIListenerCallback(
              'android-apps-info-update', {appReady: false});
          Polymer.dom.flush();
          var dialog = androidAppsPage.$$('#confirmDisableDialog');
          assertTrue(!!dialog);
          assertTrue(dialog.open);
          var actionButton =
              androidAppsPage.$$('dialog paper-button.action-button');
          assertTrue(!!actionButton);
          MockInteractions.tap(actionButton);
          Polymer.dom.flush();
          assertFalse(control.checked);
          assertTrue(managed.hidden);
        });
  });
});
