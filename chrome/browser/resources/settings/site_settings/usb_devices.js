// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'usb-devices' is the polymer element for showing the USB Devices category
 * under Site Settings.
 */

Polymer({
  is: 'usb-devices',

  behaviors: [SiteSettingsBehavior],

  properties: {
    /**
     * A list of all USB devices.
     * @type {Array<UsbDeviceEntry>}
     */
    devices: Array,
  },

  ready: function() {
    this.fetchUsbDevices_();
  },

  /**
   * Fetch the list of USB devices and update the list.
   * @private
   */
  fetchUsbDevices_: function() {
    this.browserProxy.fetchUsbDevices().then(function(deviceList) {
      this.devices = deviceList;
    }.bind(this));
  },

  /**
   * A handler when an action is selected in the action menu.
   * @param {!{model: !{item: UsbDeviceEntry}}} event
   * @private
   */
  onActionMenuIronActivate_: function(event) {
    var item = event.model.item;
    this.browserProxy.removeUsbDevice(
        item.origin, item.embeddingOrigin, item.object);
    this.fetchUsbDevices_();
  },
});
