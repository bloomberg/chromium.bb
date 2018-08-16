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
     * @return {!Promise<boolean>} Whether the operation was successful.
     */
    setFeatureEnabledState(feature, enabled) {}

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
    setFeatureEnabledState(feature, enabled) {
      return cr.sendWithPromise('setFeatureEnabledState');
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
