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
     * @param {boolean} logsUploadEnabled
     */
    startCleanup(logsUploadEnabled) {}

    /**
     * Restarts the user's computer.
     */
    restartComputer() {}

    /**
     * Hides the Cleanup page from the settings menu.
     * @param {number} source
     */
    dismissCleanupPage(source) {}

    /**
     * Updates the cleanup logs upload permission status.
     * @param {boolean} enabled
     */
    setLogsUploadPermission(enabled) {}

    /**
     * Notifies Chrome that the state of the details section changed.
     * @param {boolean} enabled
     */
    notifyShowDetails(enabled) {}

    /**
     * Notifies Chrome that the "learn more" link was clicked.
     */
    notifyLearnMoreClicked() {}
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
    startCleanup(logsUploadEnabled) {
      chrome.send('startCleanup', [logsUploadEnabled]);
    }

    /** @override */
    restartComputer() {
      chrome.send('restartComputer');
    }

    /** @override */
    dismissCleanupPage(source) {
      chrome.send('dismissCleanupPage', [source]);
    }

    /** @override */
    setLogsUploadPermission(enabled) {
      chrome.send('setLogsUploadPermission', [enabled]);
    }

    /** @override */
    notifyShowDetails(enabled) {
      chrome.send('notifyShowDetails', [enabled]);
    }

    /** @override */
    notifyLearnMoreClicked() {
      chrome.send('notifyChromeCleanupLearnMoreClicked');
    }
  }

  cr.addSingletonGetter(ChromeCleanupProxyImpl);

  return {
    ChromeCleanupProxy: ChromeCleanupProxy,
    ChromeCleanupProxyImpl: ChromeCleanupProxyImpl,
  };
});
