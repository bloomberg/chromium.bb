// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('management');
/**
 * @typedef {{
 *    name: string,
 *    permissions: !Array<string>
 * }}
 */
management.Extension;

cr.define('management', function() {
  /** @interface */
  class ManagementBrowserProxy {
    /** @return {!Promise<!Array<!management.Extension>>} */
    getExtensions() {}

    // <if expr="chromeos">
    /**
     * @return {!Promise<boolean>} Boolean describing trust root configured
     *     or not.
     */
    getLocalTrustRootsInfo() {}
    // </if>

    /**
     * @return {!Promise<!Array<string>>} The list of browser reporting info
     *     messages.
     */
    initBrowserReportingInfo() {}
  }

  /** @implements {management.ManagementBrowserProxy} */
  class ManagementBrowserProxyImpl {
    /** @override */
    getExtensions() {
      return cr.sendWithPromise('getExtensions');
    }

    // <if expr="chromeos">
    /** @override */
    getLocalTrustRootsInfo() {
      return cr.sendWithPromise('getLocalTrustRootsInfo');
    }
    // </if>

    /** @override */
    initBrowserReportingInfo() {
      return cr.sendWithPromise('initBrowserReportingInfo');
    }
  }

  cr.addSingletonGetter(ManagementBrowserProxyImpl);

  return {
    ManagementBrowserProxy: ManagementBrowserProxy,
    ManagementBrowserProxyImpl: ManagementBrowserProxyImpl
  };
});
