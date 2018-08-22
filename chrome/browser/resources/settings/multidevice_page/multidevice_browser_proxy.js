// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  class MultiDeviceBrowserProxy {
    showMultiDeviceSetupDialog() {}

    /** @return {!Promise<!MultiDevicePageContentData>} */
    getPageContentData() {}

    /**
     * @param {!settings.MultiDeviceFeature} feature The feature whose state
     *     should be set.
     * @param {boolean} enabled Whether the feature should be turned off or on.
     * @param {String} authToken Proof that the user is authenticated. Needed
     *     to enable Smart Lock, and Better Together Suite if the Smart Lock
     *     user pref is enabled.
     * @return {!Promise<boolean>} Whether the operation was successful.
     */
    setFeatureEnabledState(feature, enabled, authToken) {}

    retryPendingHostSetup() {}
  }

  /**
   * @implements {settings.MultiDeviceBrowserProxy}
   */
  class MultiDeviceBrowserProxyImpl {
    /** @override */
    showMultiDeviceSetupDialog() {
      chrome.send('showMultiDeviceSetupDialog');
    }

    /** @override */
    getPageContentData() {
      return cr.sendWithPromise('getPageContentData');
    }

    /** @override */
    setFeatureEnabledState(feature, enabled, authToken) {
      return cr.sendWithPromise(
          'setFeatureEnabledState', feature, enabled, authToken);
    }

    /** @override */
    retryPendingHostSetup() {
      chrome.send('retryPendingHostSetup');
    }
  }

  cr.addSingletonGetter(MultiDeviceBrowserProxyImpl);

  return {
    MultiDeviceBrowserProxy: MultiDeviceBrowserProxy,
    MultiDeviceBrowserProxyImpl: MultiDeviceBrowserProxyImpl,
  };
});
