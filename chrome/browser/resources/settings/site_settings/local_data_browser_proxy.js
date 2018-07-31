// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the Cookies and Local Storage Data
 * section.
 */

/**
 * @typedef {{
 *   id: string,
 *   start: number,
 *   children: !Array<CookieDetails>,
 * }}
 */
let CookieList;

/**
 * @typedef {{
 *   data: !Object,
 *   id: string,
 * }}
 */
let LocalDataItem;

/**
 * TODO(dschuyler): add |filter| and |order|.
 * @typedef {{
 *   items: !Array<!LocalDataItem>,
 *   total: number,
 * }}
 */
let LocalDataList;

/**
 * Number of cookies attached to a given domain / eTLD+1.
 * @typedef {{
 *   etldPlus1: string,
 *   numCookies: number,
 * }}
 */
let EtldPlus1CookieNumber;

cr.define('settings', function() {
  /** @interface */
  class LocalDataBrowserProxy {
    /**
     * @param {string} filter Search filter (use "" for none).
     * @return {!Promise<!LocalDataList>}
     */
    getDisplayList(filter) {}

    /**
     * Removes all local data (local storage, cookies, etc.).
     * Note: on-tree-item-removed will not be sent.
     * @return {!Promise} To signal completion.
     */
    removeAll() {}

    /**
     * Remove items that pass the current filter. Completion signaled by
     * on-tree-item-removed.
     */
    removeShownItems() {}

    /**
     * Remove a specific list item. Completion signaled by on-tree-item-removed.
     * @param {string} id Which element to delete.
     */
    removeItem(id) {}

    /**
     * Gets the cookie details for a particular site.
     * @param {string} site The name of the site.
     * @return {!Promise<!CookieList>}
     */
    getCookieDetails(site) {}

    /**
     * Gets a list containing the number of cookies for each domain (eTLD+1
     * names) given in |siteList|. This will always return a result array the
     * same length and in the same order as |siteList|.
     * @param {!Array<string>} siteList The list of sites to count cookies for.
     * @return {!Promise<!Array<!EtldPlus1CookieNumber>>}
     */
    getNumCookiesList(siteList) {}

    /**
     * Gets the plural string for a given number of cookies.
     * @param {number} numCookies The number of cookies.
     * @return {!Promise<string>}
     */
    getNumCookiesString(numCookies) {}

    /**
     * Reloads all local data.
     * TODO(dschuyler): rename function to reload().
     * @return {!Promise} To signal completion.
     */
    reloadCookies() {}

    /**
     * TODO(dschuyler): merge with removeItem().
     * Removes a given cookie.
     * @param {string} path The path to the parent cookie.
     */
    removeCookie(path) {}
  }

  /**
   * @implements {settings.LocalDataBrowserProxy}
   */
  class LocalDataBrowserProxyImpl {
    /** @override */
    getDisplayList(filter) {
      return cr.sendWithPromise('localData.getDisplayList', filter);
    }

    /** @override */
    removeAll() {
      return cr.sendWithPromise('localData.removeAll');
    }

    /** @override */
    removeShownItems() {
      chrome.send('localData.removeShownItems');
    }

    /** @override */
    removeItem(id) {
      chrome.send('localData.removeItem', [id]);
    }

    /** @override */
    getCookieDetails(site) {
      return cr.sendWithPromise('localData.getCookieDetails', site);
    }

    /** @override */
    getNumCookiesList(siteList) {
      return cr.sendWithPromise('localData.getNumCookiesList', siteList);
    }

    /** @override */
    getNumCookiesString(numCookies) {
      return cr.sendWithPromise('localData.getNumCookiesString', numCookies);
    }

    /** @override */
    reloadCookies() {
      return cr.sendWithPromise('localData.reload');
    }

    /** @override */
    removeCookie(path) {
      chrome.send('localData.removeCookie', [path]);
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(LocalDataBrowserProxyImpl);

  return {
    LocalDataBrowserProxy: LocalDataBrowserProxy,
    LocalDataBrowserProxyImpl: LocalDataBrowserProxyImpl,
  };
});
