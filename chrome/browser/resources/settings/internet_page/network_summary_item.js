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

Polymer({
  is: 'cr-network-summary-item',

  properties: {
    /**
     * True if the list is expanded.
     */
    expanded: {
      type: Boolean,
      value: false,
      observer: 'expandedChanged_'
    },

    /**
     * The maximum height in pixels for the list of networks.
     */
    maxHeight: {
      type: Number,
      value: 200
    },

    /**
     * True if this item should be hidden. We need this computed property so
     * that it can default to true, hiding this element, since no changed event
     * will be fired for deviceState if it is undefined (in CrNetworkSummary).
     */
    isHidden: {
      type: Boolean,
      value: true,
      computed: 'noDeviceState_(deviceState)'
    },

    /**
     * Device state for the network type.
     * @type {?DeviceStateProperties}
     */
    deviceState: {
      type: Object,
      value: null,
      observer: 'deviceStateChanged_'
    },

    /**
     * Network state for the active network.
     * @type {?CrOnc.NetworkStateProperties}
     */
    networkState: {
      type: Object,
      value: null
    },

    /**
     * List of all network state data for the network type.
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList: {
      type: Array,
      value: function() { return []; },
      observer: 'networkStateListChanged_'
    }
  },

  /**
   * Polymer expanded changed method.
   */
  expandedChanged_: function() {
    var type = this.deviceState ? this.deviceState.Type : '';
    this.fire('expanded', {expanded: this.expanded, type: type});
  },

  /**
   * Polymer deviceState changed method.
   */
  deviceStateChanged_: function() {
    this.updateSelectable_();
    if (!this.deviceIsEnabled_(this.deviceState))
      this.expanded = false;
  },

  /**
   * Polymer networkStateList changed method.
   */
  networkStateListChanged_: function() {
    this.updateSelectable_();
  },

  /**
   * @param {?DeviceStateProperties} deviceState The state of a device.
   * @return {boolean} True if the device state is not set.
   * @private
   */
  noDeviceState_: function(deviceState) {
    return !deviceState;
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
   * @return {string} The class value for the device enabled button.
   * @private
   */
  getDeviceEnabledButtonClass_: function(deviceState) {
    var visible = deviceState &&
        deviceState.Type != 'Ethernet' && deviceState.Type != 'VPN';
    return visible ? '' : 'invisible';
  },

  /**
   * @param {?DeviceStateProperties} deviceState The device state.
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkList
   * @return {string} The class value for the expand button.
   * @private
   */
  getExpandButtonClass_: function(deviceState, networkList) {
    var visible = this.expandIsVisible_(deviceState, networkList);
    return visible ? '' : 'invisible';
  },

  /**
   * @param {?DeviceStateProperties} deviceState The device state.
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkList
   * @return {boolean} Whether or not to show the UI to expand the list.
   * @private
   */
  expandIsVisible_: function(deviceState, networkList) {
    if (!this.deviceIsEnabled_(deviceState))
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
    var state = event.detail;
    this.fire('selected', state);
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
