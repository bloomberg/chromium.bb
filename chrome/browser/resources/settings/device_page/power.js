// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-power' is the settings subpage for power settings.
 */

Polymer({
  is: 'settings-power',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    enablePowerSettings: Boolean,

    /** @private {string} ID of the selected power source, or ''. */
    selectedPowerSourceId_: String,

    /** @private {!settings.BatteryStatus|undefined} */
    batteryStatus_: Object,

    /** @private {boolean} Whether a low-power (USB) charger is being used. */
    lowPowerCharger_: Boolean,

    /**
     * List of available dual-role power sources, if enablePowerSettings is on.
     * @private {!Array<!settings.PowerSource>|undefined}
     */
    powerSources_: Array,

    /** @private */
    powerSourceLabel_: {
      type: String,
      computed:
          'computePowerSourceLabel_(powerSources_, batteryStatus_.calculating)',
    },

    /** @private */
    showPowerSourceDropdown_: {
      type: Boolean,
      computed: 'computeShowPowerSourceDropdown_(powerSources_)',
      value: false,
    },

    /**
     * The name of the dedicated charging device being used, if present.
     * @private {string}
     */
    powerSourceName_: {
      type: String,
      computed: 'computePowerSourceName_(powerSources_, lowPowerCharger_)',
    },
  },

  /** @override */
  ready: function() {
    // enablePowerSettings comes from loadTimeData, so it will always be set
    // before attached() is called.
    if (!this.enablePowerSettings)
      settings.navigateToPreviousRoute();
  },

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'battery-status-changed', this.set.bind(this, 'batteryStatus_'));
    this.addWebUIListener(
        'power-sources-changed', this.powerSourcesChanged_.bind(this));
    settings.DevicePageBrowserProxyImpl.getInstance().updatePowerStatus();
  },

  /**
   * @param {!Array<!settings.PowerSource>|undefined} powerSources
   * @param {boolean} calculating
   * @return {string} The primary label for the power source row.
   * @private
   */
  computePowerSourceLabel_: function(powerSources, calculating) {
    return this.i18n(
        calculating ?
            'calculatingPower' :
            powerSources.length ? 'powerSourceLabel' : 'powerSourceBattery');
  },

  /**
   * @param {!Array<!settings.PowerSource>} powerSources
   * @return {boolean} True if at least one power source is attached and all of
   *     them are dual-role (no dedicated chargers).
   * @private
   */
  computeShowPowerSourceDropdown_: function(powerSources) {
    return powerSources.length > 0 && powerSources.every(function(source) {
      return source.type == settings.PowerDeviceType.DUAL_ROLE_USB;
    });
  },

  /**
   * @param {!Array<!settings.PowerSource>} powerSources
   * @param {boolean} lowPowerCharger
   * @return {string} Description of the power source.
   * @private
   */
  computePowerSourceName_: function(powerSources, lowPowerCharger) {
    if (lowPowerCharger)
      return this.i18n('powerSourceLowPowerCharger');
    if (powerSources.length)
      return this.i18n('powerSourceAcAdapter');
    return '';
  },

  onPowerSourceChange_: function() {
    settings.DevicePageBrowserProxyImpl.getInstance().setPowerSource(
        this.$$('#powerSource').value);
  },

  /**
   * @param {!Array<settings.PowerSource>} sources External power sources.
   * @param {string} selectedId The ID of the currently used power source.
   * @param {boolean} lowPowerCharger Whether the currently used power source
   *     is a low-powered USB charger.
   * @private
   */
  powerSourcesChanged_: function(sources, selectedId, lowPowerCharger) {
    this.powerSources_ = sources;
    this.selectedPowerSourceId_ = selectedId;
    this.lowPowerCharger_ = lowPowerCharger;
  },

  /**
   * @param {*} lhs
   * @param {*} rhs
   * @return {boolean}
   * @private
   */
  isEqual_: function(lhs, rhs) {
    return lhs === rhs;
  },
});
