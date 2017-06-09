// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  function ChromeCleanupProxy() {}

  ChromeCleanupProxy.prototype = {
    /**
     * Registers the current ChromeCleanupHandler as an observer of
     * ChromeCleanerController events.
     */
    registerChromeCleanerObserver: assertNotReached,

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
  };

  /**
   * @implements {settings.ChromeCleanupProxy}
   * @constructor
   */
  function ChromeCleanupProxyImpl() {}

  cr.addSingletonGetter(ChromeCleanupProxyImpl);

  ChromeCleanupProxyImpl.prototype = {
    /** @override */
    registerChromeCleanerObserver: function() {
      chrome.send('registerChromeCleanerObserver');
    },

    /** @override */
    startCleanup: function() {
      chrome.send('startCleanup');
    },

    /** @override */
    restartComputer: function() {
      chrome.send('restartComputer');
    },

    /** @override */
    dismissCleanupPage: function() {
      chrome.send('dismissCleanupPage');
    },
  };

  return {
    ChromeCleanupProxy: ChromeCleanupProxy,
    ChromeCleanupProxyImpl: ChromeCleanupProxyImpl,
  };
});
