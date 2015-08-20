// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('options');

/**
 * Copied from ash/system/chromeos/power/power_status.h.
 * @enum {number}
 */
options.PowerStatusDeviceType = {
  DEDICATED_CHARGER: 0,
  DUAL_ROLE_USB: 1,
};

/**
 * @typedef {{
 *   id: string,
 *   type: options.PowerStatusDeviceType,
 *   description: string
 * }}
 */
options.PowerSource;

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * Encapsulated handling of the power overlay.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function PowerOverlay() {
    Page.call(this, 'power-overlay',
              loadTimeData.getString('powerOverlayTabTitle'),
              'power-overlay');
  }

  cr.addSingletonGetter(PowerOverlay);

  PowerOverlay.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('power-confirm').onclick =
          PageManager.closeOverlay.bind(PageManager);
      $('power-source-dropdown').onchange =
          this.powerSourceChanged_.bind(this);
    },

    /** @override */
    didShowPage: function() {
      chrome.send('updatePowerStatus');
    },

    /**
     * @param {string} status
     * @private
     */
    setBatteryStatusText_: function(status) {
      $('battery-status-value').textContent = status;
    },

    /**
     * @param {Array<options.PowerSource>} sources External power sources.
     * @param {string} selectedId The ID of the currently used power source.
     * @param {boolean} isUsbCharger Whether the currently used power source
     *     is a USB (low-powered) charger.
     * @param {boolean} isCalculating Whether the power info is still
     *     being calculated.
     * @private
     */
    setPowerSources_: function(sources, selectedId, isUsbCharger,
                               isCalculating) {
      if (this.lastPowerSource_ != selectedId) {
        this.lastPowerSource_ = selectedId;
        if (selectedId && !isUsbCharger) {
          // It can take a while to detect a USB charger, but triggering a
          // power status update makes the determination faster.
          setTimeout(chrome.send.bind(null, 'updatePowerStatus'), 1000);
        }
      }

      var chargerRow = $('power-source-charger');

      $('power-sources').hidden = chargerRow.hidden = true;

      // If no power sources are available, only show the battery status text.
      if (sources.length == 0)
        return;

      // If we're still calculating battery time and seem to have an AC
      // adapter, the charger information may be wrong.
      if (isCalculating && selectedId && !isUsbCharger) {
        $('power-source-charger-type').textContent =
            loadTimeData.getString('calculatingPower');
        chargerRow.hidden = false;
        return;
      }

      // Check if a dedicated charger is being used.
      var usingDedicatedCharger = false;
      if (selectedId) {
        usingDedicatedCharger = sources.some(function(source) {
          return source.id == selectedId &&
              source.type == options.PowerStatusDeviceType.DEDICATED_CHARGER;
        });
      }

      if (usingDedicatedCharger) {
        // Show charger information.
        $('power-source-charger-type').textContent = loadTimeData.getString(
            isUsbCharger ? 'powerSourceLowPowerCharger' :
                           'powerSourceAcAdapter');
        chargerRow.hidden = false;
      } else {
        this.showPowerSourceList_(sources, selectedId);
      }
    },

    /**
     * Populates and shows the dropdown of available power sources.
     * @param {Array<options.PowerSource>} sources External power sources.
     * @param {string} selectedId The ID of the currently used power source.
     *     The empty string indicates no external power source is in use
     *     (running on battery).
     * @private
     */
    showPowerSourceList_: function(sources, selectedId) {
      // Clear the dropdown.
      var dropdown = $('power-source-dropdown');
      dropdown.innerHTML = '';

      // Add a battery option.
      sources.unshift({
        id: '',
        description: loadTimeData.getString('powerSourceBattery'),
      });

      // Build the power source list.
      sources.forEach(function(source) {
        var option = document.createElement('option');
        option.value = source.id;
        option.textContent = source.description;
        option.selected = source.id == selectedId;
        dropdown.appendChild(option);
      });

      // Show the power source list.
      $('power-sources').hidden = false;
    },

    /** @private */
    powerSourceChanged_: function() {
      chrome.send('setPowerSource', [$('power-source-dropdown').value]);
    },
  };

  cr.makePublic(PowerOverlay, [
      'setBatteryStatusText',
      'setPowerSources',
  ]);

  // Export
  return {
    PowerOverlay: PowerOverlay
  };
});
