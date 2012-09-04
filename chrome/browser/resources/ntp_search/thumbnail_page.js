// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp', function() {
  'use strict';

  var Tile = ntp.Tile2;
  var TilePage = ntp.TilePage2;

  /**
   * Creates a new Thumbnail object for tiling.
   * @constructor
   * @extends {Tile}
   * @extends {HTMLAnchorElement}
   * @param {Object} config Tile page configuration object.
   */
  function Thumbnail(config) {
    var el = cr.doc.createElement('a');
    el.__proto__ = Thumbnail.prototype;
    el.initialize(config);

    return el;
  }

  Thumbnail.prototype = Tile.subclass({
    __proto__: HTMLAnchorElement.prototype,

    /**
     * Initializes a Thumbnail.
     * @param {Object} config TilePage configuration object.
     */
    initialize: function(config) {
      Tile.prototype.initialize.apply(this, arguments);
      this.classList.add('thumbnail');
      this.reset();
    },

    /**
     * Thumbnail data object.
     * @type {Object}
     */
    get data() {
      return this.data_;
    },

    /**
     * Clears the DOM hierarchy for this node, setting it back to the default
     * for a blank thumbnail.
     */
    reset: function() {
      this.innerHTML =
          '<span class="thumbnail-wrapper">' +
            '<span class="thumbnail-image thumbnail-card"></span>' +
          '</span>' +
          '<span class="title"></span>';

      this.tabIndex = -1;
      this.data_ = null;
      this.title = '';
    },

    /**
     * Update the appearance of this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     */
    updateForData: function(data) {
      // TODO(pedrosimonetti): Remove data.filler usage everywhere.
      if (!data || data.filler) {
        if (this.data_)
          this.reset();
        return;
      }

      this.data_ = data;

      this.formatThumbnail_(data);
    },

    /**
     * Formats this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     * @private
     */
    formatThumbnail_: function(data) {
      var title = this.querySelector('.title');
      title.textContent = data.title;
      title.dir = data.direction;

      // Sets the tooltip.
      this.title = data.title;

      var dataUrl = data.url;
      // Allow an empty string href (e.g. for a multiple tab thumbnail).
      this.href = typeof data.href != 'undefined' ? data.href : dataUrl;

      var thumbnailImage = this.querySelector('.thumbnail-image');

      var banner = thumbnailImage.querySelector('.thumbnail-banner');
      if (banner)
        thumbnailImage.removeChild(banner);

      var favicon = thumbnailImage.querySelector('.thumbnail-favicon');
      if (favicon)
        thumbnailImage.removeChild(favicon);

      var self = this;
      var image = new Image();

      // If the thumbnail image fails to load, show the favicon and URL instead.
      // TODO(jeremycho): Move to a separate function?
      image.onerror = function() {
        banner = self.ownerDocument.createElement('div');
        banner.className = 'thumbnail-banner';

        // For now, just strip leading http://www and trailing backslash.
        // TODO(jeremycho): Consult with UX on URL truncation.
        banner.textContent = dataUrl.replace(/^(http:\/\/)?(www\.)?|\/$/gi, '');
        thumbnailImage.appendChild(banner);

        favicon = self.ownerDocument.createElement('div');
        favicon.className = 'thumbnail-favicon';
        favicon.style.backgroundImage =
            url('chrome://favicon/size/16/' + dataUrl);
        thumbnailImage.appendChild(favicon);
      }

      var thumbnailUrl = ntp.getThumbnailUrl(dataUrl);
      thumbnailImage.style.backgroundImage = url(thumbnailUrl);
      image.src = thumbnailUrl;
    },
  });

  /**
   * Creates a new ThumbnailPage object.
   * @constructor
   * @extends {TilePage}
   */
  function ThumbnailPage() {
    var el = new TilePage();
    el.__proto__ = ThumbnailPage.prototype;

    return el;
  }

  ThumbnailPage.prototype = {
    __proto__: TilePage.prototype,

    config_: {
      // The width of a cell.
      cellWidth: 132,
      // The start margin of a cell (left or right according to text direction).
      cellMarginStart: 18,
      // The border panel horizontal margin.
      bottomPanelHorizontalMargin: 100,
      // The height of the tile row.
      rowHeight: 105,
      // The maximum number of Tiles to be displayed.
      maxTileCount: 10
    },

    // Thumbnail class used in this TilePage.
    ThumbnailClass: Thumbnail,

    /**
     * Initializes a ThumbnailPage.
     */
    initialize: function() {
      this.classList.add('thumbnail-page');
      this.data_ = null;
    },

    /**
     * Create blank tiles.
     * @private
     * @param {number} count The number of Tiles to be created.
     */
    createTiles_: function(count) {
      var Class = this.ThumbnailClass;
      var config = this.config_;
      count = Math.min(count, config.maxTileCount);
      for (var i = 0; i < count; i++) {
        this.appendTile(new Class(config));
      }
    },

    /**
     * Update the tiles after a change to |data_|.
     */
    updateTiles_: function() {
      if (!this.hasBeenRendered())
        this.layout_();

      var maxTileCount = this.config_.maxTileCount;
      var data = this.data_;
      var tiles = this.tiles;
      for (var i = 0; i < maxTileCount; i++) {
        var page = data[i];
        var tile = tiles[i];

        // TODO(pedrosimonetti): What do we do when there's no tile here?
        if (!tile)
          return;

        if (i >= data.length)
          tile.reset();
        else
          tile.updateForData(page);
      }
    },

    /**
     * Array of thumbnail data objects.
     * @type {Array}
     */
    get data() {
      return this.data_;
    },
    set data(data) {
      console.error('ThumbnailPage: data_ setter is not implemented.');
    },

    /** @inheritDoc */
    shouldAcceptDrag: function(e) {
      return false;
    },
  };

  return {
    Thumbnail: Thumbnail,
    ThumbnailPage: ThumbnailPage
  };
});
