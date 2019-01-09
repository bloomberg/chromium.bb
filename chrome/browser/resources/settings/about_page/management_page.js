// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-management-page' contains scary information
 * about ChromeOS privacy.
 */
Polymer({
  is: 'settings-management-page',

  /** @private {?settings.ManagementBrowserProxy} */
  browserProxy_: null,

  /** @private */
  getDeviceManagementStatus_: function() {
    this.browserProxy_.getDeviceManagementStatus()
        .then(managedString => {
          this.$.managementStatus.textContent = managedString;
          this.$.managementStatus.hidden = false;
        })
        .catch(
            // On Chrome desktop, the behavior is to show nothing (device
            // management is outside of Chrome's control), so
            // RejectJavascriptCallback is used, which throws an error. The
            // intended handling in this case is to do nothing.
            () => {});
  },

  /** @private */
  getReportingDevice_: function() {
    this.browserProxy_.getReportingDevice().then(reportingSources => {
      if (reportingSources.length == 0) {
        return;
      }
      let reportingInfoAdded = false;

      for (const id of reportingSources) {
        reportingInfoAdded = true;
        this.$.reportingDeviceText.textContent +=
            loadTimeData.getString(id) + ' ';
      }

      if (reportingInfoAdded) {
        this.$.policies.hidden = false;
        this.$.reportingDevice.hidden = false;
      }
    });
  },

  /** @private */
  getReportingSecurity_: function() {
    this.browserProxy_.getReportingSecurity().then(reportingSources => {
      if (reportingSources.length == 0) {
        return;
      }
      let reportingInfoAdded = false;

      for (const id of reportingSources) {
        reportingInfoAdded = true;
        this.$.reportingSecurityText.textContent +=
            loadTimeData.getString(id) + ' ';
      }

      if (reportingInfoAdded) {
        this.$.policies.hidden = false;
        this.$.reportingSecurity.hidden = false;
      }
    });
  },

  /** @private */
  getReportingUserActivity_: function() {
    this.browserProxy_.getReportingUserActivity().then(reportingSources => {
      if (reportingSources.length == 0) {
        return;
      }
      let reportingInfoAdded = false;

      for (const id of reportingSources) {
        reportingInfoAdded = true;
        this.$.reportingUserActivityText.textContent +=
            loadTimeData.getString(id) + ' ';
      }

      if (reportingInfoAdded) {
        this.$.policies.hidden = false;
        this.$.reportingUserActivity.hidden = false;
      }
    });
  },

  /** @private */
  getReportingWeb_: function() {
    this.browserProxy_.getReportingWeb().then(reportingSources => {
      if (reportingSources.length == 0) {
        return;
      }
      let reportingInfoAdded = false;

      for (const id of reportingSources) {
        reportingInfoAdded = true;
        this.$.reportingWebText.textContent += loadTimeData.getString(id) + ' ';
      }

      if (reportingInfoAdded) {
        this.$.policies.hidden = false;
        this.$.reportingWeb.hidden = false;
      }
    });
  },

  /** @private */
  getExtensions_: function() {
    // Show names and permissions of |extensions| in a table.
    this.browserProxy_.getExtensions().then(extensions => {
      if (extensions.length == 0) {
        return;
      }

      const table = this.$.extensionsTable;

      for (const /** settings.Extension */ extension of extensions) {
        assert(
            extension.hasOwnProperty('permissions'),
            'Each extension must have the permissions field');
        assert(
            extension.hasOwnProperty('name'),
            'Each extension must have the name field');

        const permissionsList = document.createElement('ul');
        for (const perm of extension.permissions) {
          const permissionElement = document.createElement('li');
          permissionElement.textContent = perm;
          permissionsList.appendChild(permissionElement);
        }

        const row = table.insertRow();
        const nameCell = row.insertCell();
        // insertCell(-1) inserts at the last position.
        const permissionsCell = row.insertCell(-1);
        nameCell.textContent = extension.name;
        permissionsCell.appendChild(permissionsList);
      }

      this.$.extensions.hidden = false;
    });
  },

  /** @private */
  getLocalTrustRootsInfo_: function() {
    this.browserProxy_.getLocalTrustRootsInfo().then(trustRootsConfigured => {
      if (trustRootsConfigured) {
        this.$.trustRootsConfiguration.textContent =
            loadTimeData.getString('managementTrustRootsConfigured');
        this.$.trustRoots.hidden = false;
      }
    });
  },

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.ManagementBrowserProxyImpl.getInstance();

    this.getDeviceManagementStatus_();

    this.getReportingDevice_();

    this.getReportingSecurity_();

    this.getReportingUserActivity_();

    this.getReportingWeb_();

    this.getExtensions_();

    this.getLocalTrustRootsInfo_();
  },
});
