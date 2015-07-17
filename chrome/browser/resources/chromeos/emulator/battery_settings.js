// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var BatterySettings = Polymer({
  is: 'battery-settings',

  properties: {
    /**
     * The title for the settings section.
     */
    title: {
      type: String,
    },

    /**
     * Whether or not a battery is present in the device.
     */
    batteryPresent: {
      type: Boolean,
    },

    /**
     * The system's battery percentage.
     */
    batteryPercent: {
      type: Number,
    },

    /**
     * An integer representing the value of an
     * PowerSupplyProperties_ExternalPower enumeration.
     */
    externalPower: {
      type: Number,
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
     * A set of options for the power supply radio group. Populated via WebUI.
     * @type !Array<! { type: string, value: number }>
     */
    externalPowerOptions: {
      type: Array,
      value: function() { return []; }
    }
  },

  ready: function() {
    this.title = 'Battery/Power Settings';
  },

  observers: [
    'batteryPercentChanged(batteryPercent)',
    'externalPowerChanged(externalPower)',
  ],

  batteryPercentChanged: function(percent) {
    chrome.send('updateBatteryPercent', [parseInt(percent)]);
  },

  setBatteryPercent: function(percent) {
    this.batteryPercent = percent;
  },

  setExternalPowerOptions: function(options) {
    this.externalPowerOptions = options;
  },

  externalPowerChanged: function(source) {
    var index = -1;

    // Find the index of the selected power source.
    for (var i = 0; i < this.externalPowerOptions.length; i++) {
      if (this.externalPowerOptions[i].text == source) {
        index = i;
        break;
      }
    }

    if (index >= 0)
      chrome.send('updateExternalPower', [index]);
  }
});
