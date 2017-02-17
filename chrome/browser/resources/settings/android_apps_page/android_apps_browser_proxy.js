// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the "Google Play Store" (ARC) section
 * to retrieve information about android apps.
 */

/**
 * @typedef {{appReady: boolean}}
 * @see chrome/browser/ui/webui/settings/chromeos/android_apps_handler.cc
 */
var AndroidAppsInfo;

cr.define('settings', function() {
  /** @interface */
  function AndroidAppsBrowserProxy() {
  }

  AndroidAppsBrowserProxy.prototype = {
    requestAndroidAppsInfo: function() {},

    /**
     * @param {boolean} keyboardAction True if the app was opened using a
     *     keyboard action.
     */
    showAndroidAppsSettings: function(keyboardAction) {},
  };

  /**
   * @constructor
   * @implements {settings.AndroidAppsBrowserProxy}
   */
  function AndroidAppsBrowserProxyImpl() {
  }

  // The singleton instance_ can be replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(AndroidAppsBrowserProxyImpl);

  AndroidAppsBrowserProxyImpl.prototype = {
    /** @override */
    requestAndroidAppsInfo: function() {
      chrome.send('requestAndroidAppsInfo');
    },

    /** @override */
    showAndroidAppsSettings: function(keyboardAction) {
      chrome.send('showAndroidAppsSettings', [keyboardAction]);
    },
  };

  return {
    AndroidAppsBrowserProxy: AndroidAppsBrowserProxy,
    AndroidAppsBrowserProxyImpl: AndroidAppsBrowserProxyImpl,
  };
});
