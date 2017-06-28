// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.AndroidAppsBrowserProxy}
 * @extends {TestBrowserProxy}
 */
function TestAndroidAppsBrowserProxy() {
  TestBrowserProxy.call(this, [
    'requestAndroidAppsInfo',
    'showAndroidAppsSettings',
  ]);
}

TestAndroidAppsBrowserProxy.prototype = {
  __proto__: TestBrowserProxy.prototype,

  /** @override */
  requestAndroidAppsInfo: function() {
    this.methodCalled('requestAndroidAppsInfo');
    this.setAndroidAppsState(false, false);
  },

  /** override */
  showAndroidAppsSettings: function(keyboardAction) {
    this.methodCalled('showAndroidAppsSettings');
  },

  setAndroidAppsState: function(playStoreEnabled, settingsAppAvailable) {
    // We need to make sure to pass a new object here, otherwise the property
    // change event may not get fired in the listener.
    var appsInfo = {
      playStoreEnabled: playStoreEnabled,
      settingsAppAvailable: settingsAppAvailable,
    };
    cr.webUIListenerCallback('android-apps-info-update', appsInfo);
  },
};
