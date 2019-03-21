// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Kiosk Next Home API implementation.
 */

/**
 * Builds fake App from package name.
 * TODO(brunoad): Remove when this info is available for real.
 * @return {!kioskNextHome.App} that can be used to launch apps via
 *     arcAppsPrivate.
 */
function buildApp(packageName) {
  return {
    appId: packageName,
    type: kioskNextHome.AppType.ARC,
    displayName: packageName,
    packageName: packageName,
    readiness: kioskNextHome.AppReadiness.READY,
    thumbnailImage: '',
  };
}

/** @implements {kioskNextHome.Bridge} */
class KioskNextHomeBridge {
  constructor() {
    /**
     * @private
     * @const {!Array<!kioskNextHome.Listener>}
     */
    this.listeners_ = [];
    /** @private */
    this.identityAccessorProxy_ = new identity.mojom.IdentityAccessorProxy();

    const kioskNextHomeInterfaceBrokerProxy =
        chromeos.kioskNextHome.mojom.KioskNextHomeInterfaceBroker.getProxy();
    kioskNextHomeInterfaceBrokerProxy.getIdentityAccessor(
        this.identityAccessorProxy_.$.createRequest());

    chrome.arcAppsPrivate.onInstalled.addListener(installedApp => {
      for (const listener of this.listeners_) {
        listener.onAppChanged(buildApp(installedApp.packageName));
      }
    });

    window.addEventListener(
        'online',
        () => this.notifyNetworkStateChange(kioskNextHome.NetworkState.ONLINE));
    window.addEventListener(
        'offline',
        () =>
            this.notifyNetworkStateChange(kioskNextHome.NetworkState.OFFLINE));
  }

  /** @override */
  addListener(listener) {
    this.listeners_.push(listener);
  }

  /** @override */
  getAccountId() {
    return this.identityAccessorProxy_.getPrimaryAccountWhenAvailable().then(
        account => account.accountInfo.gaia);
  }

  /** @override */
  getAccessToken(scopes) {
    return this.identityAccessorProxy_.getPrimaryAccountWhenAvailable()
        .then(account => {
          return this.identityAccessorProxy_.getAccessToken(
              account.accountInfo.accountId, {'scopes': scopes},
              'kiosk_next_home');
        })
        .then(tokenInfo => {
          if (tokenInfo.token) {
            return tokenInfo.token;
          }
          throw 'Unable to get access token.';
        });
  }

  /** @override */
  getAndroidId() {
    // TODO(brunoad): Implement this method.
    return Promise.reject('Not implemented.');
  }

  /** @override */
  getApps() {
    // TODO(ltenorio): Use app controller call.
    return new Promise((resolve, reject) => {
      chrome.arcAppsPrivate.getLaunchableApps(launchableApps => {
        const installedApps = [];
        for (const launchableApp of launchableApps) {
          installedApps.push(buildApp(launchableApp.packageName));
        }
        resolve(installedApps);
      });
    });
  }

  /** @override */
  launchApp(appId) {
    // TODO(brunoad): Use app service call.
    chrome.arcAppsPrivate.launchApp(appId);
    return Promise.resolve();
  }

  /** @override */
  launchIntent(intent) {
    // TODO(brunoad): Implement this method.
    return Promise.reject('Not implemented.');
  }

  /** @override */
  uninstallApp(appId) {
    // TODO(brunoad): Implement this method.
    return Promise.reject('Not implemented.');
  }

  /** @override */
  getNetworkState() {
    return navigator.onLine ? kioskNextHome.NetworkState.ONLINE :
                              kioskNextHome.NetworkState.OFFLINE;
  }

  /**
   * Notifies listeners about changes in network connection state.
   * @param {kioskNextHome.NetworkState} networkState Indicates current network
   *     state.
   */
  notifyNetworkStateChange(networkState) {
    for (const listener of this.listeners_) {
      listener.onNetworkStateChanged(networkState);
    }
  }
}

/**
 * Provides bridge implementation.
 * @return {!kioskNextHome.Bridge} Bridge instance that can be used to interact
 *     with Chrome OS.
 */
kioskNextHome.getChromeOsBridge = function() {
  return new KioskNextHomeBridge();
};
