// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used for Internet page. */
cr.exportPath('settings');

cr.define('settings', function() {
  /** @interface */
  class InternetPageBrowserProxy {
    /**
     *  Shows configuration of connnected external VPN network.
     *  @param {string} guid
     */
    showNetworkConfigure(guid) {}

    /**
     * Sends add VPN request to external VPN provider.
     * @param {string} networkType
     * @param {string} appId
     */
    addThirdPartyVpn(networkType, appId) {}
  }

  /**
   * @implements {settings.InternetPageBrowserProxy}
   */
  class InternetPageBrowserProxyImpl {
    /** @override */
    showNetworkConfigure(guid) {
      chrome.send('configureNetwork', [guid]);
    }

    /** @override */
    addThirdPartyVpn(networkType, appId) {
      chrome.send('addNetwork', [networkType, appId]);
    }
  }

  cr.addSingletonGetter(InternetPageBrowserProxyImpl);

  return {
    InternetPageBrowserProxy: InternetPageBrowserProxy,
    InternetPageBrowserProxyImpl: InternetPageBrowserProxyImpl,
  };
});
