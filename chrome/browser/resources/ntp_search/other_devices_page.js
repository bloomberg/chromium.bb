// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The page that shows other devices and their tabs.
 */

cr.define('ntp', function() {
  'use strict';

  var Tile = ntp.Tile;
  var TilePage = ntp.TilePage;

  /**
   * Device type strings that are used as a part of the class name for the
   * device picture element.
   * @enum {string}
   */
  var ICON_TYPES = {
    LAPTOP: 'laptop',
    PHONE: 'phone',
    TABLET: 'tablet',
  };

  /**
   * Map for converting device types to pictures.
   * @type {Object}
   * @const
   */
  var DEVICE_ICONS = {
    'win': ICON_TYPES.LAPTOP,
    'macosx': ICON_TYPES.LAPTOP,
    'linux': ICON_TYPES.LAPTOP,
    'chromeos': ICON_TYPES.LAPTOP,
    'other': ICON_TYPES.LAPTOP,
    'phone': ICON_TYPES.PHONE,
    'tablet': ICON_TYPES.TABLET,
  };

  /**
   * Creates a new OtherDevice object for tiling.
   * @constructor
   * @extends {Tile}
   * @extends {HTMLAnchorElement}
   * @param {Object} config Tile page configuration object.
   */
  function OtherDevice(config) {
    var el = cr.doc.createElement('div');
    el.__proto__ = OtherDevice.prototype;
    el.initialize(config);

    return el;
  }

  OtherDevice.prototype = Tile.subclass({
    __proto__: HTMLDivElement.prototype,

    /**
     * Initializes an OtherDevice Tile.
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

      // Ask the native side to show a popup listing the devices's tabs.
      chrome.send('showOtherDeviceSessionPopup',
                  [this.data.tag, e.screenX, e.screenY]);
    },

    /**
     * Clears the DOM hierarchy for this node, setting it back to the default
     * for a blank tile.
     */
    reset: function() {
      this.innerHTML =
          '<div class="device-image"></div>' +
          '<div class="devices-infopane">' +
            '<div class="devices-label-container">' +
              '<p class="devices-info-text devices-name"></p>' +
              '<p class="devices-info-text devices-timestamp"></p>' +
            '</div>' +
          '</div>';
      // TODO(vadimt): Replace assigning null with calling base.reset().
      this.data_ = null;
    },

    /**
     * Gets the class for the picture element for a device of a given type.
     * @param {string} deviceType Type of the device.
     * @private
     */
    getPictureClass_: function(deviceType) {
      var iconType = DEVICE_ICONS[deviceType] || ICON_TYPES.LAPTOP;
      // TODO(vadimt): Also need Retina-resolution images.
      return 'device-image device-' + iconType;
    },

    /**
     * Update the appearance of this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     */
    setData: function(data) {
      // TODO(vadimt): We should decide what to do with too long texts.
      Tile.prototype.setData.apply(this, arguments);

      var image = this.querySelector('.device-image');
      image.className = this.getPictureClass_(data.deviceType);
      var deviceName = this.querySelector('.devices-name');
      deviceName.textContent = data.name;
      var timestamp = this.querySelector('.devices-timestamp');
      timestamp.textContent = data.modifiedTime;
    },
  });

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

    TileClass: OtherDevice,

    // TODO(vadimt): config constants are not final.
    // TODO(vadimt): center tiles horizontally.
    /**
     * Configuration object for the tile.
     * @type {Object}
     * @const
     * @private
     */
    config_: {
      // The width of a cell.
      cellWidth: 161,
      // The start margin of a cell (left or right according to text direction).
      cellMarginStart: 18,
      // The border panel horizontal margin.
      bottomPanelHorizontalMargin: 100,
      // The height of the tile row.
      // TODO(vadimt): Strange that we need to repeat same value for all pages.
      rowHeight: 105,
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
     */
    setDataList: function(dataList) {
      // TODO(vadimt): show text to right/left of image depending on locale.
      var startTime = Date.now();

      TilePage.prototype.setDataList.apply(this, arguments);

      var dot = this.navigationDot;
      if (this.dataList_.length == 0) {
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
