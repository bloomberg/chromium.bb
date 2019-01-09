// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');
/**
 * @typedef {{
 *    name: string,
 *    permissions: !Array<string>
 * }}
 */
settings.Extension;

cr.define('settings', function() {
  /** @interface */
  class ManagementBrowserProxy {
    /**
     * @return {!Promise<string>} Return title of the management page
     */
    getManagementTitle() {}

    /**
     * @return {!Promise<string>} Message stating if device is enterprise
     * managed and by whom.
     */
    getDeviceManagementStatus() {}

    /**
     * @return {!Promise<!Array<string>>} Types of device reporting.
     */
    getReportingDevice() {}

    /**
     * @return {!Promise<!Array<string>>} Types of device reporting.
     */
    getReportingSecurity() {}

    /**
     * @return {!Promise<!Array<string>>} Types of device reporting.
     */
    getReportingUserActivity() {}

    /**
     * @return {!Promise<!Array<string>>} Types of device reporting.
     */
    getReportingWeb() {}

    /**
     * Each extension has a name and a list of permission messages.
     * @return {!Promise<!Array<!settings.Extension>>} List of extensions.
     */
    getExtensions() {}

    /**
     * @return {!Promise<boolean>} Boolean describing trust root configured
     * or not.
     */
    getLocalTrustRootsInfo() {}
  }

  /**
   * @implements {settings.ManagementBrowserProxy}
   */
  class ManagementBrowserProxyImpl {
    /** @override */
    getManagementTitle() {
      return cr.sendWithPromise('getManagementTitle');
    }

    /** @override */
    getDeviceManagementStatus() {
      return cr.sendWithPromise('getDeviceManagementStatus');
    }

    /** @override */
    getReportingDevice() {
      return cr.sendWithPromise('getReportingDevice');
    }

    /** @override */
    getReportingSecurity() {
      return cr.sendWithPromise('getReportingSecurity');
    }

    /** @override */
    getReportingUserActivity() {
      return cr.sendWithPromise('getReportingUserActivity');
    }

    /** @override */
    getReportingWeb() {
      return cr.sendWithPromise('getReportingWeb');
    }

    /** @override */
    getExtensions() {
      return cr.sendWithPromise('getExtensions');
    }

    /** @override */
    getLocalTrustRootsInfo() {
      return cr.sendWithPromise('getLocalTrustRootsInfo');
    }
  }

  // Make Page a singleton.
  cr.addSingletonGetter(ManagementBrowserProxyImpl);

  return {
    ManagementBrowserProxy: ManagementBrowserProxy,
    ManagementBrowserProxyImpl: ManagementBrowserProxyImpl
  };
});
