// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DevicesPage and DevicesView, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('devices_page', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;

  /**
   * Page that contains a header and a DevicesView.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function DevicesPage() {
    Page.call(this, 'devices', 'Devices', 'devices');

    this.deviceTable = new device_table.DeviceTable();
    this.pageDiv.appendChild(this.deviceTable);
  }

  DevicesPage.prototype = {
    __proto__: Page.prototype,

    /**
     * Sets the device collection for the page's device table.
     * @param {!device_collection.DeviceCollection} devices
     */
    setDevices: function(devices) {
      this.deviceTable.setDevices(devices);
    }
  };

  return {
    DevicesPage: DevicesPage
  };
});
