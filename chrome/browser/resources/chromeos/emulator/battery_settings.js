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
    },

    /**
     * A string representing the value of an
     * PowerSupplyProperties_ExternalPower enumeration.
     */
    externalPower: {
      type: String,
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
    },

    /**
     * A string representing the time left until the battery is at 100%.
     */
    timeUntilFull: {
      type: String,
    },

    /**
     * The title for the settings section.
     */
    title: {
      type: String,
    },
  },

  ready: function() {
    this.title = 'Power Settings';
  },

  observers: [
    'batteryPercentChanged(batteryPercent)',
    'externalPowerChanged(externalPower)',
    'timeUntilEmptyChanged(timeUntilEmpty)',
    'timeUntilFullChanged(timeUntilFull)',
  ],

  batteryPercentChanged: function(percent) {
    chrome.send('updateBatteryPercent', [parseInt(percent)]);
  },

  externalPowerChanged: function(source) {
    var index = -1;

    // Find the index of the selected power source.
    for (var i = 0; i < this.externalPowerOptions.length; i++) {
      if (this.externalPowerOptions[i] == source) {
        index = i;
        break;
      }
    }

    if (index >= 0)
      chrome.send('updateExternalPower', [index]);
  },

  timeUntilEmptyChanged: function(time) {
    chrome.send('updateTimeToEmpty', [parseInt(time)]);
  },

  timeUntilFullChanged: function(time) {
    chrome.send('updateTimeToFull', [parseInt(time)]);
  },

  updatePowerProperties: function(percent, external_power, empty, full) {
    this.batteryPercent = percent;
    this.externalPower = this.externalPowerOptions[external_power];
    this.timeUntilEmpty = empty;
    this.timeUntilFull = full;
  }
});
