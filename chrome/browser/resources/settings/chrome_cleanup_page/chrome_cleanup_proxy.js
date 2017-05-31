// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  function ChromeCleanupProxy() {}

  ChromeCleanupProxy.prototype = {
    /**
     * Registers the current ChromeCleanupHandler as an observer of
     * ChromeCleanupController events.
     */
    registerChromeCleanupObserver: assertNotReached,

    /**
     * Starts a cleanup on the user's computer.
     */
    startCleanup: assertNotReached,

    /**
     * Restarts the user's computer.
     */
    restartComputer: assertNotReached,

    /**
     * Hides the Cleanup page from the settings menu.
     */
    dismissCleanupPage: assertNotReached,

    /**
     * Retrieves if there is cleanup information to present to the user.
     * @return {!Promise<boolean>}
     */
    getChromeCleanupVisibility: assertNotReached,
  };

  /**
   * @implements {settings.ChromeCleanupProxy}
   * @constructor
   */
  function ChromeCleanupProxyImpl() {}

  cr.addSingletonGetter(ChromeCleanupProxyImpl);

  ChromeCleanupProxyImpl.prototype = {
    /** @override */
    registerChromeCleanupObserver: function() {
      // TODO(proberge): Uncomment once this is implemented.
      // chrome.send('registerChromeCleanupObserver');
    },

    /** @override */
    startCleanup: function() {
      // TODO(proberge): Uncomment once this is implemented.
      // chrome.send('startCleanup');
    },

    /** @override */
    restartComputer: function() {
      // TODO(proberge): Uncomment once this is implemented.
      // chrome.send('restartComputer');
    },

    /** @override */
    dismissCleanupPage: function() {
      // TODO(proberge): Uncomment once this is implemented.
    },

    /** @override */
    getChromeCleanupVisibility: function() {
      // TODO(proberge): Uncomment once this is implemented.
      // return cr.sendWithPromise('getChromeCleanupVisibility');
      return Promise.resolve(false);
    },
  };

  return {
    ChromeCleanupProxy: ChromeCleanupProxy,
    ChromeCleanupProxyImpl: ChromeCleanupProxyImpl,
  };
});
