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

cr.define('cleanup', function() {
  /** @interface */
  function CleanupBrowserProxy() {}

  CleanupBrowserProxy.prototype = {
    /**
     * Fetch the result of the latest Chrome Cleanup Tool scanning task.
     * @return {!Promise<LastScanResult>}
     */
    requestLastScanResult: function() {},
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
  };

  return {
    CleanupBrowserProxy: CleanupBrowserProxy,
    CleanupBrowserProxyImpl: CleanupBrowserProxyImpl,
  }
});
