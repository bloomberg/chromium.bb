// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-bluetooth-page' is the settings page for managing bluetooth
 *  properties and devices.
 *
 * Example:
 *    <core-animated-pages>
 *      <settings-bluetooth-page>
 *      </settings-bluetooth-page>
 *      ... other pages ...
 *    </core-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-bluetooth-page
 */
Polymer({
  is: 'settings-bluetooth-page',

  properties: {
    /** The current active route. */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /** Whether bluetooth is enabled. */
    bluetoothEnabled: Boolean,
  },

  /**
   * Listener for chrome.bluetooth.onAdapterStateChanged events.
   * @type {function(!chrome.bluetooth.AdapterState)}
   * @private
   */
  bluetoothAdapterStateChangedListener_: function() {},

  /** @override */
  attached: function() {
    this.bluetoothAdapterStateChangedListener_ =
        this.onBluetoothAdapterStateChanged_.bind(this);
    chrome.bluetooth.onAdapterStateChanged.addListener(
        this.bluetoothAdapterStateChangedListener_);

  },

  /** @override */
  detached: function() {
    chrome.bluetooth.onAdapterStateChanged.removeListener(
        this.bluetoothAdapterStateChangedListener_);
  },

  /**
   * Event called when a user action changes the bluetoothEnabled state.
   * @private
   */
  onBluetoothEnabledChange_: function() {
    chrome.bluetoothPrivate.setAdapterState(
        {powered: this.bluetoothEnabled}, function() {
          if (chrome.runtime.lastError) {
            console.error(
                'Error enabling bluetooth:', chrome.runtime.lastError.message);
          }
        });
  },

  /**
   * Process bluetooth.onAdapterStateChanged events.
   * @param {!chrome.bluetooth.AdapterState} state
   * @private
   */
  onBluetoothAdapterStateChanged_: function(state) {
    this.bluetoothEnabled = state.powered;
  },
});
