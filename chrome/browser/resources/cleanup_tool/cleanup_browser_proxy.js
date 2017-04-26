// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper object and related behavior that encapsulate messaging
 * between JS and C++ for interacting with the Chrome Cleanup Tool.
 */

/**
 * @typedef {{
 *   hasScanResults: boolean,
 *   isInfected: boolean,
 *   detectionStatusText: string,
 *   detectionTimeText: string,
 * }}
 */
var LastScanResult;

/**
 * @typedef {{
 *   wasCancelled: boolean,
 *   uwsRemoved: Array<string>,
 *   requiresReboot: boolean,
 * }}
 */
var CleanupResult;

cr.define('cleanup', function() {
  /** @interface */
  function CleanupBrowserProxy() {}

  CleanupBrowserProxy.prototype = {
    /**
     * Fetch the result of the latest Chrome Cleanup Tool scanning task.
     * @return {!Promise<LastScanResult>}
     */
    requestLastScanResult: function() {},
    /**
     * Attempts to run the Chrome Cleanup Tool in scanning mode.
     * @return {!Promise<LastScanResult>}
     */
    startScan: function() {},

    /**
     * Opens the prompt to run the Chrome Cleanup Tool.
     * @return {!Promise<CleanupResult>}
     */
    startCleanup: function() {},
  };

  /**
   * @constructor
   * @implements {cleanup.CleanupBrowserProxy}
   */
  function CleanupBrowserProxyImpl() {}
  cr.addSingletonGetter(CleanupBrowserProxyImpl);

  CleanupBrowserProxyImpl.prototype = {
    /** @override */
    requestLastScanResult: function() {
      return cr.sendWithPromise('requestLastScanResult');
    },
    /** @override */
    startScan: function() {
      return cr.sendWithPromise('startScan');
    },
    /** @override */
    startCleanup: function() {
      return cr.sendWithPromise('startCleanup');
    },
  };

  return {
    CleanupBrowserProxy: CleanupBrowserProxy,
    CleanupBrowserProxyImpl: CleanupBrowserProxyImpl,
  }
});
