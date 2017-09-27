// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  class LifetimeBrowserProxy {
    // Triggers a browser restart.
    restart() {}

    // Triggers a browser relaunch.
    relaunch() {}

    // <if expr="chromeos">
    // First signs out current user and then performs a restart.
    signOutAndRestart() {}

    /**
     * Triggers a factory reset. The parameter indicates whether to install a
     * TPM firmware update (if available) after the reset.
     *
     * @param {boolean=} requestTpmFirmwareUpdate
     */
    factoryReset(requestTpmFirmwareUpdate) {}
    // </if>
  }

  /**
   * @implements {settings.LifetimeBrowserProxy}
   */
  class LifetimeBrowserProxyImpl {
    /** @override */
    restart() {
      chrome.send('restart');
    }

    /** @override */
    relaunch() {
      chrome.send('relaunch');
    }

    // <if expr="chromeos">
    /** @override */
    signOutAndRestart() {
      chrome.send('signOutAndRestart');
    }

    /** @override */
    factoryReset(requestTpmFirmwareUpdate) {
      chrome.send('factoryReset', [requestTpmFirmwareUpdate]);
    }
    // </if>
  }

  cr.addSingletonGetter(LifetimeBrowserProxyImpl);

  return {
    LifetimeBrowserProxy: LifetimeBrowserProxy,
    LifetimeBrowserProxyImpl: LifetimeBrowserProxyImpl,
  };
});
