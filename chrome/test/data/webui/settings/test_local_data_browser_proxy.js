// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A test version of LocalDataBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 *
 * @implements {settings.LocalDataBrowserProxy}
 */
class TestLocalDataBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getCookieDetails',
      'reloadCookies',
      'removeCookie',
    ]);

    /** @private {?CookieList} */
    this.cookieDetails_ = null;
  }

  /** @override */
  getCookieDetails(site) {
    this.methodCalled('getCookieDetails', site);
    return Promise.resolve(this.cookieDetails_ || {id: '', children: []});
  }

  /**
   * @param {!CookieList} cookieList
   */
  setCookieDetails(cookieList) {
    this.cookieDetails_ = cookieList;
  }

  /** @override */
  reloadCookies() {
    this.methodCalled('reloadCookies');
    return Promise.resolve({id: null, children: []});
  }

  /** @override */
  removeCookie(path) {
    this.methodCalled('removeCookie', path);
  }
}
