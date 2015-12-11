// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var BatterySettings = Polymer({
  is: 'battery-settings',

  properties: {
    /** The system's battery percentage. */
    batteryPercent: Number,

    /**
     * A string representing a value in the
     * PowerSupplyProperties_BatteryState enumeration.
     */
    batteryState: {
      type: String,
      observer: 'batteryStateChanged',
    },

    /**
     * An array representing the battery state options.
     * The names are ordered based on the
     * PowerSupplyProperties_BatteryState enumeration. These values must be
     * in sync.
     */
    batteryStateOptions: {
      type: Array,
      value: function() { return ['Full', 'Charging', 'Discharging',
                                  'Not Present']; },
    },

    /**
     * A string representing a value in the
     * PowerSupplyProperties_ExternalPower enumeration.
     */
    externalPower: {
      type: String,
      observer: 'externalPowerChanged',
    },

    /**
     * An array representing the external power options.
     * The names are ordered based on the
     * PowerSupplyProperties_ExternalPower enumeration. These values must be
     * in sync.
     */
    externalPowerOptions: {
      type: Array,
      value: function() { return ['AC', 'USB (Low Power)', 'Disconnected']; }
    },

    /** A string representing the time left until the battery is discharged. */
    timeUntilEmpty: String,

    /** A string representing the time left until the battery is at 100%. */
    timeUntilFull: String,

    /** The title for the settings section. */
    title: {
      type: String,
      value: 'Power',
    },
  },

  initialize: function() {
    if (!this.initialized) {
      chrome.send('requestPowerInfo');
      this.initialized = true;
    }
  },

  onBatteryPercentChange: function(e) {
    this.percent = parseInt(e.target.value);
    if (!isNaN(this.percent))
      chrome.send('updateBatteryPercent', [this.percent]);
  },

  batteryStateChanged: function(batteryState) {
    // Find the index of the selected battery state.
    var index = this.batteryStateOptions.indexOf(batteryState);
    if (index < 0)
      return;
    chrome.send('updateBatteryState', [index]);
  },

  externalPowerChanged: function(externalPower) {
    // Find the index of the selected power source.
    var index = this.externalPowerOptions.indexOf(externalPower);
    if (index < 0)
      return;
    chrome.send('updateExternalPower', [index]);
  },

  onTimeUntilEmptyChange: function(e) {
    this.timeUntilEmpty = parseInt(e.target.value);
    if (!isNaN(this.timeUntilEmpty))
      chrome.send('updateTimeToEmpty', [this.timeUntilEmpty]);
  },

  onTimeUntilFullChange: function(e) {
    this.timeUntilFull = parseInt(e.target.value);
    if (!isNaN(this.timeUntilFull))
      chrome.send('updateTimeToFull', [this.timeUntilFull]);
  },

  updatePowerProperties: function(power_properties) {
    this.batteryPercent = power_properties.battery_percent;
    this.batteryState =
        this.batteryStateOptions[power_properties.battery_state];
    this.externalPower =
        this.externalPowerOptions[power_properties.external_power];
    this.timeUntilEmpty = power_properties.battery_time_to_empty_sec;
    this.timeUntilFull = power_properties.battery_time_to_full_sec;
  },

  isBatteryPresent: function() {
    return this.batteryState != 'Not Present';
  },
});
