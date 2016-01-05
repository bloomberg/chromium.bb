// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-bluetooth-add-device-dialog' is the settings subpage for adding
 * bluetooth devices.
 *
 * @group Chrome Settings Elements
 * @element settings-bluetooth-add-device-dialog
 */
Polymer({
  is: 'settings-bluetooth-add-device-dialog',

  properties: {
    /**
     * The cached bluetooth adapter state.
     * @type {!chrome.bluetooth.AdapterState|undefined}
     */
    adapterState: {
      type: Object,
      observer: 'adapterStateChanged_',
    },

    /**
     * The ordered list of bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     */
    deviceList: {
      type: Array,
      value: function() { return []; },
    },
  },

  /** @private */
  adapterStateChanged_: function() {
    if (!this.adapterState.powered)
      this.fire('close-dialog');
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {boolean}
   * @private
   */
  deviceNotPaired_: function(device) {
    return !device.paired;
  },

  /**
   * @param {!Array<!chrome.bluetooth.Device>} deviceList
   * @return {boolean} True if deviceList contains any unpaired devices.
   * @private
   */
  haveDevices_: function(deviceList) {
    return this.deviceList.findIndex(function(d) { return !d.paired; }) != -1;
  },

  /**
   * @param {!{detail: {action: string, device: !chrome.bluetooth.Device}}} e
   * @private
   */
  onDeviceEvent_: function(e) {
    this.fire('device-event', e.detail);
    /** @type {Event} */(e).stopPropagation();
  },

  /** @private */
  onCancelTap_: function() { this.fire('close-dialog'); },
});
