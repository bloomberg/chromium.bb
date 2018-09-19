// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('management', function() {
  /**
   * A singleton object that handles communication between browser and WebUI.
   */
  class Page {
    constructor() {
      /** @private {!ManagementBrowserProxy} */
      this.browserProxy_ = ManagementBrowserProxyImpl.getInstance();
    }

    /**
     * Main initialization function. Called by the browser on page load.
     */
    initialize() {
      // Notify the browser that the page has loaded, causing it to send the
      // management data.

      // Show whether device is managed or not, and the management domain in the
      // former case.
      this.browserProxy_.getDeviceManagementStatus()
          .then(function(managedString) {
            document.body.querySelector('#management-status > p').textContent =
                managedString;
          })
          .catch(
              // On Chrome desktop, the behavior is to show nothing (device
              // management is outside of Chrome's control), so
              // RejectJavascriptCallback is used, which throws an error. The
              // intended handling in this case is to do nothing.
              () => {});

      // Show descriptions of the types of reporting in the |reportingSources|
      // list.
      this.browserProxy_.getReportingInfo().then(function(reportingSources) {
        if (reportingSources.length == 0)
          return;

        $('device-configuration').hidden = false;

        for (const id of reportingSources) {
          const element = document.createElement('li');
          element.textContent = loadTimeData.getString(id);
          $('reporting-info-list').appendChild(element);
        }
      });
    }
  }

  /** @interface */
  class ManagementBrowserProxy {
    /**
     * @return {!Promise<string>} Message stating if device is enterprise
     * managed and by whom.
     */
    getDeviceManagementStatus() {}

    /**
     * @return {!Promise<!Array<string>>} Types of device reporting.
     */
    getReportingInfo() {}
  }

  /**
   * @implements {ManagementBrowserProxy}
   */
  class ManagementBrowserProxyImpl {
    /** @override */
    getDeviceManagementStatus() {
      return cr.sendWithPromise('getDeviceManagementStatus');
    }

    /** @override */
    getReportingInfo() {
      return cr.sendWithPromise('getReportingInfo');
    }
  }

  // Make Page a singleton.
  cr.addSingletonGetter(Page);
  cr.addSingletonGetter(ManagementBrowserProxyImpl);

  return {Page: Page};
});

// Have the main initialization function be called when the page finishes
// loading.
document.addEventListener(
    'DOMContentLoaded', () => management.Page.getInstance().initialize());
