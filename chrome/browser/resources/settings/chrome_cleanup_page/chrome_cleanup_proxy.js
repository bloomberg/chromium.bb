// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  class ChromeCleanupProxy {
    /**
     * Registers the current ChromeCleanupHandler as an observer of
     * ChromeCleanerController events.
     */
    registerChromeCleanerObserver() {}

    /**
     * Starts a cleanup on the user's computer.
     */
    startCleanup() {}

    /**
     * Restarts the user's computer.
     */
    restartComputer() {}

    /**
     * Hides the Cleanup page from the settings menu.
     */
    dismissCleanupPage() {}
  }

  /**
   * @implements {settings.ChromeCleanupProxy}
   */
  class ChromeCleanupProxyImpl {
    /** @override */
    registerChromeCleanerObserver() {
      chrome.send('registerChromeCleanerObserver');
    }

    /** @override */
    startCleanup() {
      chrome.send('startCleanup');
    }

    /** @override */
    restartComputer() {
      chrome.send('restartComputer');
    }

    /** @override */
    dismissCleanupPage() {
      chrome.send('dismissCleanupPage');
    }
  }

  cr.addSingletonGetter(ChromeCleanupProxyImpl);

  return {
    ChromeCleanupProxy: ChromeCleanupProxy,
    ChromeCleanupProxyImpl: ChromeCleanupProxyImpl,
  };
});
