// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the Cookies and Local Storage Data
 * section.
 */

cr.define('settings', function() {
  /** @interface */
  class LocalDataBrowserProxy {
    /**
     * Gets the cookie details for a particular site.
     * @param {string} site The name of the site.
     * @return {!Promise<!CookieList>}
     */
    getCookieDetails(site) {}

    /**
     * Reloads all cookies.
     * @return {!Promise<!CookieList>} Returns the full cookie
     *     list.
     */
    reloadCookies() {}

    /**
     * Fetches all children of a given cookie.
     * @param {string} path The path to the parent cookie.
     * @return {!Promise<!Array<!CookieDataSummaryItem>>} Returns a cookie list
     *     for the given path.
     */
    loadCookieChildren(path) {}

    /**
     * Removes a given cookie.
     * @param {string} path The path to the parent cookie.
     */
    removeCookie(path) {}

    /**
     * Removes all cookies.
     * @return {!Promise<!CookieList>} Returns the up to date cookie list once
     *     deletion is complete (empty list).
     */
    removeAllCookies() {}
  }

  /**
   * @implements {settings.LocalDataBrowserProxy}
   */
  class LocalDataBrowserProxyImpl {
    /** @override */
    getCookieDetails(site) {
      return cr.sendWithPromise('getCookieDetails', site);
    }

    /** @override */
    reloadCookies() {
      return cr.sendWithPromise('reloadCookies');
    }

    /** @override */
    loadCookieChildren(path) {
      return cr.sendWithPromise('loadCookie', path);
    }

    /** @override */
    removeCookie(path) {
      chrome.send('removeCookie', [path]);
    }

    /** @override */
    removeAllCookies() {
      return cr.sendWithPromise('removeAllCookies');
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
