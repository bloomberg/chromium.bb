// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var BatterySettings = Polymer({
  is: 'battery-settings',

  properties: {
    /**
     * The system's battery percentage.
     */
    batteryPercent: {
      type: Number,
      observer: 'batteryPercentChanged',
    },

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
      value: function() { return ['Full', 'Charging', 'Disharging',
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

    /**
     * A string representing the time left until the battery is discharged.
     */
    timeUntilEmpty: {
      type: String,
      observer: 'timeUntilEmptyChanged',
    },

    /**
     * A string representing the time left until the battery is at 100%.
     */
    timeUntilFull: {
      type: String,
      observer: 'timeUntilFullChanged',
    },

    /**
     * The title for the settings section.
     */
    title: {
      type: String,
    },
  },

  ready: function() {
    this.title = 'Power';
  },

  initialize: function() {
    if (!this.initialized) {
      chrome.send('requestPowerInfo');
      this.initialized = true;
    }
  },

  batteryPercentChanged: function(percent, oldPercent) {
    if (oldPercent != undefined)
      chrome.send('updateBatteryPercent', [parseInt(percent)]);
  },

  batteryStateChanged: function(state) {
    // Find the index of the selected battery state.
    var index = this.batteryStateOptions.indexOf(state);
    if (index >= 0)
      chrome.send('updateBatteryState', [index]);
  },

  externalPowerChanged: function(source) {
    // Find the index of the selected power source.
    var index = this.externalPowerOptions.indexOf(source);
    if (index >= 0)
      chrome.send('updateExternalPower', [index]);
  },

  timeUntilEmptyChanged: function(time, oldTime) {
    if (oldTime != undefined)
      chrome.send('updateTimeToEmpty', [parseInt(time)]);
  },

  timeUntilFullChanged: function(time, oldTime) {
    if (oldTime != undefined)
      chrome.send('updateTimeToFull', [parseInt(time)]);
  },

  updatePowerProperties: function(power_properties) {
    this.batteryPercent = power_properties.battery_percent;
    this.batteryState =
        this.batteryStateOptions[power_properties.battery_state];
    this.externalPower =
        this.externalPowerOptions[power_properties.external_power];
    this.timeUntilEmpty = power_properties.battery_time_to_empty_sec;
    this.timeUntilFull = power_properties.battery_time_to_full_sec;
  }
});
