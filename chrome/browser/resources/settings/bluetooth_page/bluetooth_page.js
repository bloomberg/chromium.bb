// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'Settings page for managing bluetooth properties and devices. This page
 * just provodes a summary and link to the subpage.
 */

var bluetoothApis = bluetoothApis || {
  /**
   * Set this to provide a fake implementation for testing.
   * @type {Bluetooth}
   */
  bluetoothApiForTest: null,

  /**
   * Set this to provide a fake implementation for testing.
   * @type {BluetoothPrivate}
   */
  bluetoothPrivateApiForTest: null,
};

Polymer({
  is: 'settings-bluetooth-page',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Reflects the current state of the toggle buttons (in this page and the
     * subpage). This will be set when the adapter state change or when the user
     * changes the toggle.
     * @private
     */
    bluetoothToggleState_: {
      type: Boolean,
      observer: 'bluetoothToggleStateChanged_',
    },

    /**
     * Set to true before the adapter state is received, when the adapter is
     * unavailable, and while an adapter state change is requested. This
     * prevents user changes while a change is in progress or when the adapter
     * is not available.
     * @private
     */
    bluetoothToggleDisabled_: {
      type: Boolean,
      value: true,
    },

    /**
     * The cached bluetooth adapter state.
     * @type {!chrome.bluetooth.AdapterState|undefined}
     * @private
     */
    adapterState_: {
      type: Object,
      notify: true,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        var map = new Map();
        if (settings.routes.BLUETOOTH_DEVICES) {
          map.set(
              settings.routes.BLUETOOTH_DEVICES.path,
              '#bluetoothDevices .subpage-arrow');
        }
        return map;
      },
    },

    /**
     * Interface for bluetooth calls. May be overriden by tests.
     * @type {Bluetooth}
     * @private
     */
    bluetooth: {
      type: Object,
      value: chrome.bluetooth,
    },

    /**
     * Interface for bluetoothPrivate calls. May be overriden by tests.
     * @type {BluetoothPrivate}
     * @private
     */
    bluetoothPrivate: {
      type: Object,
      value: chrome.bluetoothPrivate,
    },
  },

  observers: ['deviceListChanged_(deviceList_.*)'],

  /**
   * Listener for chrome.bluetooth.onAdapterStateChanged events.
   * @type {function(!chrome.bluetooth.AdapterState)|undefined}
   * @private
   */
  bluetoothAdapterStateChangedListener_: undefined,

  /** @override */
  ready: function() {
    if (bluetoothApis.bluetoothApiForTest)
      this.bluetooth = bluetoothApis.bluetoothApiForTest;
    if (bluetoothApis.bluetoothPrivateApiForTest)
      this.bluetoothPrivate = bluetoothApis.bluetoothPrivateApiForTest;
  },

  /** @override */
  attached: function() {
    this.bluetoothAdapterStateChangedListener_ =
        this.onBluetoothAdapterStateChanged_.bind(this);
    this.bluetooth.onAdapterStateChanged.addListener(
        this.bluetoothAdapterStateChangedListener_);

    // Request the inital adapter state.
    this.bluetooth.getAdapterState(this.bluetoothAdapterStateChangedListener_);
  },

  /** @override */
  detached: function() {
    if (this.bluetoothAdapterStateChangedListener_) {
      this.bluetooth.onAdapterStateChanged.removeListener(
          this.bluetoothAdapterStateChangedListener_);
    }
  },

  /**
   * @return {string}
   * @private
   */
  getIcon_: function() {
    if (!this.bluetoothToggleState_)
      return 'settings:bluetooth-disabled';
    return 'settings:bluetooth';
  },

  /**
   * @param {boolean} enabled
   * @param {string} onstr
   * @param {string} offstr
   * @return {string}
   * @private
   */
  getOnOffString_: function(enabled, onstr, offstr) {
    return enabled ? onstr : offstr;
  },

  /**
   * Process bluetooth.onAdapterStateChanged events.
   * @param {!chrome.bluetooth.AdapterState} state
   * @private
   */
  onBluetoothAdapterStateChanged_: function(state) {
    this.adapterState_ = state;
    this.bluetoothToggleState_ = state.powered;
    this.bluetoothToggleDisabled_ = !state.available;
  },

  /** @private */
  onTap_: function() {
    if (this.adapterState_.available === false)
      return;
    if (!this.bluetoothToggleState_)
      this.bluetoothToggleState_ = true;
    else
      this.openSubpage_();
  },

  /**
   * @param {!Event} e
   * @private
   */
  stopTap_: function(e) {
    e.stopPropagation();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onSubpageArrowTap_: function(e) {
    this.openSubpage_();
    e.stopPropagation();
  },

  /** @private */
  bluetoothToggleStateChanged_: function() {
    if (!this.adapterState_ || this.bluetoothToggleDisabled_ ||
        this.bluetoothToggleState_ == this.adapterState_.powered) {
      return;
    }
    this.bluetoothToggleDisabled_ = true;
    this.bluetoothPrivate.setAdapterState(
        {powered: this.bluetoothToggleState_}, () => {
          var error = chrome.runtime.lastError;
          if (error && error != 'Error setting adapter properties: powered') {
            console.error('Error enabling bluetooth: ' + error.message);
            return;
          }
          this.setPrefValue(
              'ash.user.bluetooth.adapter_enabled',
              this.bluetoothToggleState_);
        });
  },

  /** @private */
  openSubpage_: function() {
    settings.navigateTo(settings.routes.BLUETOOTH_DEVICES);
  }
});
