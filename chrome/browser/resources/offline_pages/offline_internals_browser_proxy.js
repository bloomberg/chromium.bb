// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   onlineUrl: string,
 *   creationTime: number,
 *   id: string,
 *   namespace: string,
 *   size: string,
 *   filePath: string,
 *   lastAccessTime: number,
 *   accessCount: number
 * }}
 */
var OfflinePage;

/**
 * @typedef {{
 *   status: string,
 *   onlineUrl: string,
 *   creationTime: number,
 *   id: string,
 *   namespace: string,
 *   lastAttempt: number
 * }}
 */
var SavePageRequest;

cr.define('offlineInternals', function() {
  /** @interface */
  function OfflineInternalsBrowserProxy() {}

  OfflineInternalsBrowserProxy.prototype = {
    /**
     * Gets current list of stored pages.
     * @return {!Promise<!Array<OfflinePage>>} A promise firing when the
     *     list is fetched.
     */
    getStoredPages: function() {},

    /**
     * Gets current offline queue requests.
     * @return {!Promise<!Array<SavePageRequest>>} A promise firing when the
     *     request queue is fetched.
     */
    getRequestQueue: function() {},

    /**
     * Deletes all the pages in stored pages.
     * @return {!Promise<!string>} A promise firing when the pages are deleted.
     */
    deleteAllPages: function() {},

    /**
     * Deletes a set of pages from stored pages
     * @param {!Array<string>} ids A list of page IDs to delete.
     * @return {!Promise<!string>} A promise firing when the selected
     *     pages are deleted.
     */
    deleteSelectedPages: function(ids) {},
  };

  /**
   * @constructor
   * @implements {offlineInternals.OfflineInternalsBrowserProxy}
   */
  function OfflineInternalsBrowserProxyImpl() {}
  cr.addSingletonGetter(OfflineInternalsBrowserProxyImpl);

  OfflineInternalsBrowserProxyImpl.prototype = {
    /** @override */
    getStoredPages: function() {
      return cr.sendWithPromise('getStoredPages');
    },

    /** @override */
    getRequestQueue: function() {
      return cr.sendWithPromise('getRequestQueue');
    },

    /** @override */
    deleteAllPages: function() {
      return cr.sendWithPromise('deleteAllPages');
    },

    /** @override */
    deleteSelectedPages: function(ids) {
      return cr.sendWithPromise('deleteSelectedPages', ids);
    }
  };

  return {
    OfflineInternalsBrowserProxy: OfflineInternalsBrowserProxy,
    OfflineInternalsBrowserProxyImpl: OfflineInternalsBrowserProxyImpl
  };
});
