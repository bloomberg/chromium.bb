// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Clear browsing data" dialog
 * to interact with the browser.
 */

/**
 * An ImportantSite represents a domain with data that the user might want
 * to protect from being deleted. ImportantSites are determined based on
 * various engagement factors, such as whether a site is bookmarked or receives
 * notifications.
 *
 * @typedef {{
 *   registerableDomain: string,
 *   reasonBitfield: number,
 *   exampleOrigin: string,
 *   isChecked: boolean,
 *   storageSize: number,
 *   hasNotifications: boolean
 * }}
 */
var ImportantSite;

cr.define('settings', function() {
  /** @interface */
  class ClearBrowsingDataBrowserProxy {
    /**
     * @param {!Array<!ImportantSite>} importantSites
     * @return {!Promise<void>}
     *     A promise resolved when data clearing has completed.
     */
    clearBrowsingData(importantSites) {}

    /**
     * @return {!Promise<!Array<!ImportantSite>>}
     *     A promise resolved when imporant sites are retrieved.
     */
    getImportantSites() {}

    /**
     * Kick off counter updates and return initial state.
     * @return {!Promise<void>} Signal when the setup is complete.
     */
    initialize() {}
  }

  /**
   * @implements {settings.ClearBrowsingDataBrowserProxy}
   */
  class ClearBrowsingDataBrowserProxyImpl {
    /** @override */
    clearBrowsingData(importantSites) {
      return cr.sendWithPromise('clearBrowsingData', importantSites);
    }

    /** @override */
    getImportantSites() {
      return cr.sendWithPromise('getImportantSites');
    }

    /** @override */
    initialize() {
      return cr.sendWithPromise('initializeClearBrowsingData');
    }
  }

  cr.addSingletonGetter(ClearBrowsingDataBrowserProxyImpl);

  return {
    ClearBrowsingDataBrowserProxy: ClearBrowsingDataBrowserProxy,
    ClearBrowsingDataBrowserProxyImpl: ClearBrowsingDataBrowserProxyImpl,
  };
});
