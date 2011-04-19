// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {
  'use strict';

  var TilePage = ntp4.TilePage;

  /**
   * Creates a new Most Visited object for tiling.
   * @constructor
   * @extends {HTMLAnchorElement}
   */
  function MostVisited() {
    var el = cr.doc.createElement('a');
    el.__proto__ = MostVisited.prototype;
    el.initialize();

    return el;
  }

  MostVisited.prototype = {
    __proto__: HTMLAnchorElement.prototype,

    initialize: function() {
      this.reset();
    },

    /**
     * Clears the DOM hierarchy for this node, setting it back to the default
     * for a blank thumbnail.
     */
    reset: function() {
      this.className = 'most-visited filler';
      // TODO(estade): why do we need edit-mode-border?
      this.innerHTML =
          '<div class="edit-mode-border fills-parent">' +
            '<div class="edit-bar-wrapper">' +
              '<div class="edit-bar">' +
                '<div class="pin"></div>' +
                '<div class="spacer"></div>' +
                '<div class="remove"></div>' +
              '</div>' +
            '</div>' +
            '<span class="thumbnail-wrapper fills-parent">' +
              '<span class="thumbnail fills-parent"></span>' +
            '</span>' +
            '<span class="title"></span>' +
          '</div>';

      this.tabIndex = -1;
    },

    /**
     * Update the appearance of this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     */
    updateForData: function(data) {
      if (data.filler) {
        this.reset();
        return;
      }

      this.tabIndex = 0;
      this.classList.remove('filler');
      var thumbnailUrl = data.thumbnailUrl || 'chrome://thumb/' + data.url;
      this.querySelector('.thumbnail').style.backgroundImage =
          url(thumbnailUrl);
    },

    /**
     * Set the size and position of the most visited tile.
     * @param {number} size The total size of |this|.
     * @param {number} x The x-position.
     * @param {number} y The y-position.
     *     animate.
     */
    setBounds: function(size, x, y) {
      this.style.width = this.style.height = size + 'px';
      this.style.left = x + 'px';
      this.style.top = y + 'px';
    },
  };

  var mostVisitedPageGridValues = {
    // The fewest tiles we will show in a row.
    minColCount: 2,
    // The most tiles we will show in a row.
    maxColCount: 4,

    // TODO(estade): Change these to real values.
    // The smallest a tile can be.
    minTileWidth: 200,
    // The biggest a tile can be.
    maxTileWidth: 240,
  };
  TilePage.initGridValues(mostVisitedPageGridValues);

  var THUMBNAIL_COUNT = 8;

  /**
   * Creates a new MostVisitedPage object.
   * @param {string} name The display name for the page.
   * @constructor
   * @extends {TilePage}
   */
  function MostVisitedPage(name) {
    var el = new TilePage(name, mostVisitedPageGridValues);
    el.__proto__ = MostVisitedPage.prototype;
    el.initialize();

    return el;
  }

  MostVisitedPage.prototype = {
    __proto__: TilePage.prototype,

    initialize: function() {
      this.classList.add('most-visited-page');

      this.data_ = null;
      this.mostVisitedTiles_ = this.getElementsByClassName('most-visited');
    },

    /**
     * Create blank (filler) tiles.
     * @private
     */
    createTiles_: function() {
      for (var i = 0; i < THUMBNAIL_COUNT; i++) {
        this.appendTile(new MostVisited());
      }
    },

    /**
     * Update the tiles after a change to |data_|.
     */
    updateTiles_: function() {
      for (var i = 0; i < THUMBNAIL_COUNT; i++) {
        var page = this.data_[i];
        var tile = this.mostVisitedTiles_[i];

        if (i >= this.data_.length)
          tile.reset();
        else
          tile.updateForData(page);
      }
    },

    /**
     * Array of most visited data objects.
     * @type {Array}
     */
    get data() {
      return this.data_;
    },
    set data(data) {
      // The first time data is set, create the tiles.
      if (!this.data_)
        this.createTiles_();

      // We append the class name with the "filler" so that we can style fillers
      // differently.
      this.data_ = data.slice(0, THUMBNAIL_COUNT);
      this.updateTiles_();
    },
  };

  return {
    MostVisitedPage: MostVisitedPage,
  };
});
