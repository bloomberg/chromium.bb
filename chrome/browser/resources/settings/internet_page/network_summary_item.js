// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the network state for a specific
 * type and a list of networks for that type.
 */
(function() {

/** @typedef {chrome.networkingPrivate.DeviceStateProperties} */
var DeviceStateProperties;

Polymer('cr-network-summary-item', {
  publish: {
    /**
     * True if the list is expanded.
     *
     * @attribute expanded
     * @type {boolean}
     * @default false
     */
    expanded: false,

    /**
     * The maximum height in pixels for the list of networks.
     *
     * @attribute maxHeight
     * @type {number}
     * @default 200
     */
    maxHeight: 200,

    /**
     * Device state for the network type.
     *
     * @attribute deviceState
     * @type {?DeviceStateProperties}
     * @default null
     */
    deviceState: null,

    /**
     * Network state for the active network.
     *
     * @attribute networkState
     * @type {?CrOncDataElement}
     * @default null
     */
    networkState: null,

    /**
     * List of all network state data for the network type.
     *
     * @attribute networkStateList
     * @type {?Array<!CrOncDataElement>}
     * @default null
     */
    networkStateList: null,
  },

  /**
   * Polymer expanded changed method.
   */
  expandedChanged: function() {
    var type = this.deviceState ? this.deviceState.Type : '';
    this.fire('expanded', {expanded: this.expanded, type: type});
  },

  /**
   * Polymer deviceState changed method.
   */
  deviceStateChanged: function() {
    this.updateSelectable_();
    if (!this.deviceIsEnabled_(this.deviceState))
      this.expanded = false;
  },

  /**
   * Polymer networkStateList changed method.
   */
  networkStateListChanged: function() {
    this.updateSelectable_();
  },

  /**
   * @param {?DeviceStateProperties} deviceState The state of a device.
   * @return {boolean} Whether or not the device state is enabled.
   * @private
   */
  deviceIsEnabled_: function(deviceState) {
    return deviceState && deviceState.State == 'Enabled';
  },

  /**
   * @param {?DeviceStateProperties} deviceState The device state.
   * @return {boolean} Whether or not to show the UI to enable the network.
   * @private
   */
  deviceEnabledIsVisible_: function(deviceState) {
    return deviceState &&
        deviceState.Type != 'Ethernet' && deviceState.Type != 'VPN';
  },

  /**
   * @param {?DeviceStateProperties} deviceState The device state.
   * @param {?Array<!CrOncDataElement>} networkList A list of networks.
   * @return {boolean} Whether or not to show the UI to expand the list.
   * @private
   */
  expandIsVisible_: function(deviceState, networkList) {
    if (!this.deviceIsEnabled_(deviceState) || !networkList)
      return false;
    var minLength = (this.type == 'WiFi') ? 1 : 2;
    return networkList.length >= minLength;
  },

  /**
   * Event triggered when the details div is clicked on.
   * @param {!Object} event The enable button event.
   * @private
   */
  onDetailsClicked_: function(event) {
    if ((event.target.id == 'expandListButton') ||
        (this.deviceState && !this.deviceIsEnabled_(this.deviceState))) {
      // Already handled or disabled, do nothing.
      return;
    }
    if (this.expandIsVisible_(this.deviceState, this.networkStateList)) {
      // Expandable, toggle expand.
      this.expanded = !this.expanded;
      return;
    }
    // Not expandable, fire 'selected' with |networkState|.
    this.fire('selected', this.networkState);
  },

  /**
   * Event triggered when a cr-network-item is the network list is selected.
   * @param {!{detail: CrNetworkListItem}} event
   * @private
   */
  onListItemSelected_: function(event) {
    var onc = event.detail;
    this.fire('selected', onc);
  },

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Object} event The enable button event.
   * @private
   */
  onDeviceEnabledToggled_: function(event) {
    var deviceIsEnabled = this.deviceIsEnabled_(this.deviceState);
    var type = this.deviceState ? this.deviceState.Type : '';
    this.fire('device-enabled-toggled',
              {enabled: !deviceIsEnabled, type: type});
    // Make sure this does not propagate to onDetailsClicked_.
    event.stopPropagation();
  },

  /**
   * Called whenever the 'selectable' state might change.
   * @private
   */
  updateSelectable_: function() {
    var selectable = this.deviceIsEnabled_(this.deviceState);
    this.$.details.classList.toggle('selectable', selectable);
  },
});
})();
