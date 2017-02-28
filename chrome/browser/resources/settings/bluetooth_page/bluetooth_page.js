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

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    bluetoothEnabled_: {
      type: Boolean,
      observer: 'bluetoothEnabledChanged_',
      notify: true,
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
    if (!this.bluetoothEnabled_)
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
    this.bluetoothEnabled_ = state.powered;
  },

  /** @private */
  onTap_: function() {
    if (this.adapterState_.available === false)
      return;
    if (!this.bluetoothEnabled_)
      this.bluetoothEnabled_ = true;
    else
      this.openSubpage_();
  },

  /**
   * @param {Event} e
   * @private
   */
  stopTap_: function(e) {
    e.stopPropagation();
  },

  /**
   * @param {Event} e
   * @private
   */
  onSubpageArrowTap_: function(e) {
    this.openSubpage_();
    e.stopPropagation();
  },

  /** @private */
  bluetoothEnabledChanged_: function() {
    this.bluetoothPrivate.setAdapterState(
        {powered: this.bluetoothEnabled_}, function() {
          if (chrome.runtime.lastError) {
            console.error(
                'Error enabling bluetooth: ' +
                chrome.runtime.lastError.message);
          }
        });
  },

  /** @private */
  openSubpage_: function() {
    settings.navigateTo(settings.Route.BLUETOOTH_DEVICES);
  }
});
