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
  function ClearBrowsingDataBrowserProxy() {}

  ClearBrowsingDataBrowserProxy.prototype = {
    /**
     * @param {!Array<!ImportantSite>} importantSites
     * @return {!Promise<void>}
     *     A promise resolved when data clearing has completed.
     */
    clearBrowsingData: function(importantSites) {},

    /**
     * @return {!Promise<!Array<!ImportantSite>>}
     *     A promise resolved when imporant sites are retrieved.
     */
    getImportantSites: function() {},

    /**
     * Kick off counter updates and return initial state.
     * @return {!Promise<void>} Signal when the setup is complete.
     */
    initialize: function() {},
  };

  /**
   * @constructor
   * @implements {settings.ClearBrowsingDataBrowserProxy}
   */
  function ClearBrowsingDataBrowserProxyImpl() {}
  cr.addSingletonGetter(ClearBrowsingDataBrowserProxyImpl);

  ClearBrowsingDataBrowserProxyImpl.prototype = {
    /** @override */
    clearBrowsingData: function(importantSites) {
      return cr.sendWithPromise('clearBrowsingData', importantSites);
    },

    /** @override */
    getImportantSites: function() {
      return cr.sendWithPromise('getImportantSites');
    },

    /** @override */
    initialize: function() {
      return cr.sendWithPromise('initializeClearBrowsingData');
    },
  };

  return {
    ClearBrowsingDataBrowserProxy: ClearBrowsingDataBrowserProxy,
    ClearBrowsingDataBrowserProxyImpl: ClearBrowsingDataBrowserProxyImpl,
  };
});
