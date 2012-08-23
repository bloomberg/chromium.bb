// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp', function() {
  'use strict';

  var Thumbnail = ntp.Thumbnail;
  var ThumbnailPage = ntp.ThumbnailPage;

  /**
   * Creates a new Recently Closed object for tiling.
   * @constructor
   * @extends {Thumbnail}
   * @extends {HTMLAnchorElement}
   * @param {Object} config Tile page configuration object.
   */
  function RecentlyClosed(config) {
    var el = cr.doc.createElement('a');
    el.__proto__ = RecentlyClosed.prototype;
    el.initialize(config);

    return el;
  }

  RecentlyClosed.prototype = {
    __proto__: Thumbnail.prototype,

    /**
     * Initializes a RecentlyClosed Thumbnail.
     * @param {Object} config TilePage configuration object.
     */
    initialize: function(config) {
      Thumbnail.prototype.initialize.apply(this, arguments);
      this.addEventListener('click', this.handleClick_);
    },

    /**
     * Formats this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     * @private
     */
    formatThumbnail_: function(data) {
      var newData = {};
      if (data.type == 'window') {
        newData.title = formatTabsText(data.tabs.length);
        newData.direction = isRTL() ? 'rtl' : 'ltr';
        newData.href = '';

        // For now, we show the thumbnail of the first tab of the closed window.
        // TODO(jeremycho): Show a stack of thumbnails instead with the last
        // focused thumbnail on top.
        newData.url = data.tabs[0].url;
      } else {
        newData = data;
      }
      Thumbnail.prototype.formatThumbnail_.call(this, newData);
    },

    /**
     * Handles a click on the tile.
     * @param {Event} e The click event.
     * @private
     */
    handleClick_: function(e) {
      var data = this.data;
      chrome.send('recordAppLaunchByURL',
                  [encodeURIComponent(data.url),
                  ntp.APP_LAUNCH.NTP_RECENTLY_CLOSED]);
      chrome.send('reopenTab', [data.sessionId, this.index,
          e.button, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey]);
    },
  };

  /**
   * Creates a new RecentlyClosedPage object.
   * @constructor
   * @extends {ThumbnailPage}
   */
  function RecentlyClosedPage() {
    var el = new ThumbnailPage();
    el.__proto__ = RecentlyClosedPage.prototype;
    el.initialize();

    return el;
  }

  RecentlyClosedPage.prototype = {
    __proto__: ThumbnailPage.prototype,

    ThumbnailClass: RecentlyClosed,

    /**
     * Initializes a RecentlyClosed Thumbnail.
     * @param {Object} config TilePage configuration object.
     */
    initialize: function(config) {
      ThumbnailPage.prototype.initialize.apply(this, arguments);
      this.classList.add('recently-closed-page');
    },

    /**
     * Sets the data that will be used to create Thumbnails.
     * @param {Array} data The array of data.
     * @type (Array}
     */
    set data(data) {
      var startTime = Date.now();
      var maxTileCount = this.config_.maxTileCount;

      this.data_ = data.slice(0, maxTileCount);
      var dataLength = data.length;
      var tileCount = this.tileCount;
      // Create new tiles if necessary.
      // TODO(jeremycho): Verify this handles the removal of tiles, once that
      // functionality is supported.
      if (tileCount < dataLength)
        this.createTiles_(dataLength - tileCount);

      this.updateTiles_();
      // Only show the dot if we have recently closed tabs to display.
      var dot = this.navigationDot;
      if (dot)
        dot.hidden = dataLength == 0;

      logEvent('recentlyClosed.layout: ' + (Date.now() - startTime));
    }
  };

  /**
   * Returns the text used for a recently closed window.
   * @param {number} numTabs Number of tabs in the window.
   * @return {string} The text to use.
   */
  function formatTabsText(numTabs) {
    if (numTabs == 1)
      return loadTimeData.getString('closedwindowsingle');
    return loadTimeData.getStringF('closedwindowmultiple', numTabs);
  }

  return {
    RecentlyClosedPage: RecentlyClosedPage,
  };
});
