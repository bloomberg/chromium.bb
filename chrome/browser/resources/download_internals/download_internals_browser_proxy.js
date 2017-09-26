// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   serviceState: string,
 *   modelStatus: string,
 *   driverStatus: string,
 *   fileMonitorStatus: string
 * }}
 */
var ServiceStatus;

cr.define('downloadInternals', function() {
  /** @interface */
  class DownloadInternalsBrowserProxy {
    /**
     * Gets the current status of the Download Service.
     * @return {!Promise<ServiceStatus>} A promise firing when the service
     *     status is fetched.
     */
    getServiceStatus() {}
  }

  /**
   * @implements {downloadInternals.DownloadInternalsBrowserProxy}
   */
  class DownloadInternalsBrowserProxyImpl {
    /** @override */
    getServiceStatus() {
      return cr.sendWithPromise('getServiceStatus');
    }
  }

  cr.addSingletonGetter(DownloadInternalsBrowserProxyImpl);

  return {
    DownloadInternalsBrowserProxy: DownloadInternalsBrowserProxy,
    DownloadInternalsBrowserProxyImpl: DownloadInternalsBrowserProxyImpl
  };
});