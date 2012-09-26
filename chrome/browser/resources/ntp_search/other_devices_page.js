// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The page that shows other devices and their tabs.
 */

cr.define('ntp', function() {
  'use strict';

  var Tile = ntp.Tile2;
  var TilePage = ntp.TilePage2;

  /**
   * Creates a new OtherDevice object for tiling.
   * @constructor
   * @extends {Tile}
   * @extends {HTMLAnchorElement}
   * @param {Object} config Tile page configuration object.
   */
  function OtherDevice(config) {
    var el = cr.doc.createElement('a');
    el.__proto__ = OtherDevice.prototype;
    el.initialize(config);

    return el;
  }

  OtherDevice.prototype = {
    __proto__: Tile.prototype,

    /**
     * Initializes a OtherDevice Tile.
     * @param {Object} config TilePage configuration object.
     */
    initialize: function(config) {
      Tile.prototype.initialize.apply(this, arguments);

      this.addEventListener('click', this.handleClick_);
      this.addEventListener('carddeselected', this.handleCardDeselected_);
    },

    /**
     * Handles a click on the tile.
     * @param {Event} e The click event.
     * @private
     */
    handleClick_: function(e) {
      // Records the index of this tile.
      // TODO(vadimt): 8 is the maximum value for the histogram. Replace with a
      // correct number for devices.
      chrome.send('metricsHandler:recordInHistogram',
                  ['NewTabPage.OtherDevice', this.index, 8]);
    },
  };

  /**
   * Creates a new OtherDevicesPage object.
   * @constructor
   * @extends {TilePage}
   */
  function OtherDevicesPage() {
    var el = new TilePage();
    el.__proto__ = OtherDevicesPage.prototype;
    el.initialize();

    return el;
  }

  OtherDevicesPage.prototype = {
    __proto__: TilePage.prototype,

    // TODO(vadimt): config constants are not final.
    config_: {
      // The width of a cell.
      cellWidth: 100,
      // The start margin of a cell (left or right according to text direction).
      cellMarginStart: 18,
      // The border panel horizontal margin.
      bottomPanelHorizontalMargin: 100,
      // The height of the tile row.
      rowHeight: 100,
      // The maximum number of Tiles to be displayed.
      maxTileCount: 10
    },

    /**
     * Initializes a OtherDevicesPage.
     */
    initialize: function() {
      TilePage.prototype.initialize.apply(this, arguments);

      this.classList.add('other-devices-page');
    },

    /**
     * Handles the 'card deselected' event (i.e. the user clicked to another
     * pane).
     * @private
     * @param {Event} e The CardChanged event.
     */
    handleCardDeselected_: function(e) {
      if (!document.documentElement.classList.contains('starting-up')) {
        chrome.send('deviceAction',
                    [ntp.NtpFollowAction.CLICKED_OTHER_NTP_PANE]);
      }
    },

    /**
     * Sets the data that will be used to create and update Tiles.
     * @param {Array} data The array of data.
     * @type (Array}
     */
    setData: function(data, isTabSyncEnabled) {
      // TODO(vadimt): unused parameter isTabSyncEnabled.
      var startTime = Date.now();

      // TODO(vadimt): Uncomment the line below when setDataList is implemented.
      //TilePage.prototype.setDataList.apply(this, arguments);

      var dot = this.navigationDot;
      if (data.length == 0) {
        dot.hidden = true;
        // If the device list is empty and we're still on the Other Devices
        // page, fall back to the first page in the dot list.
        var cardSlider = ntp.getCardSlider();
        if (cardSlider.currentCardValue == this)
          cardSlider.selectCard(0, true);
      } else {
        dot.hidden = false;
      }

      logEvent('devices.layout: ' + (Date.now() - startTime));
    },
  };

  return {
    OtherDevicesPage: OtherDevicesPage,
  };
});
