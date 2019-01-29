// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-management-page' contains scary information
 * about ChromeOS privacy.
 */
Polymer({
  is: 'settings-management-page',

  behaviors: [I18nBehavior],

  properties: {
    /** @private {!Array<string>} */
    reportingDevice_: Array,

    /** @private {!Array<string>} */
    reportingSecurity_: Array,

    /** @private {!Array<string>} */
    reportingUserActivity_: Array,

    /** @private {!Array<string>} */
    reportingWeb_: Array,
  },

  /** @private {?settings.ManagementBrowserProxy} */
  browserProxy_: null,

  /**
   * @return {boolean}
   * @private
   */
  shouldHidePolicies_: function() {
    return !this.reportingDevice_.length && !this.reportingSecurity_.length &&
        !this.reportingUserActivity_.length && !this.reportingWeb_.length;
  },

  /** @private */
  getDeviceManagementStatus_: function() {
    this.browserProxy_.getDeviceManagementStatus()
        .then(managedString => {
          this.$.managementStatusInfo.firstChild.data = managedString + ' ';
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
        nameCell.className = 'extension-name';
        // insertCell(-1) inserts at the last position.
        const permissionsCell = row.insertCell(-1);
        permissionsCell.className = 'extension-permissions';
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

    this.browserProxy_.getReportingDevice().then(reportingSources => {
      this.reportingDevice_ = reportingSources;
    });

    this.browserProxy_.getReportingSecurity().then(reportingSources => {
      this.reportingSecurity_ = reportingSources;
    });

    this.browserProxy_.getReportingUserActivity().then(reportingSources => {
      this.reportingUserActivity_ = reportingSources;
    });

    this.browserProxy_.getReportingWeb().then(reportingSources => {
      this.reportingWeb_ = reportingSources;
    });

    this.getExtensions_();

    this.getLocalTrustRootsInfo_();
  },
});
