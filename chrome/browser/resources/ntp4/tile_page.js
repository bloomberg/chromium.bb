// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {
  'use strict';

  /**
   * Gives the proportion of the row width that is devoted to a single icon.
   * @param {number} rowTileCount The number of tiles in a row.
   * @return {number} The ratio between icon width and row width.
   */
  function tileWidthFraction(rowTileCount) {
    return rowTileCount +
        (rowTileCount - 1) * TILE_SPACING_FRACTION;
  }

  /**
   * Calculates an assortment of tile-related values for a grid with the
   * given dimensions.
   * @param {number} width The pixel width of the grid.
   * @param {number} numRowTiles The number of tiles in a row.
   * @return {Object} A mapping of pixel values.
   */
  function tileValuesForGrid(width, numRowTiles) {
    var tileWidth = width / tileWidthFraction(numRowTiles);
    var offsetX = tileWidth * (1 + TILE_SPACING_FRACTION);
    var interTileSpacing = offsetX - tileWidth;

    return {
      tileWidth: tileWidth,
      offsetX: offsetX,
      interTileSpacing: interTileSpacing,
    };
  }

  // The proportion of the tile width which will be used as spacing between
  // tiles.
  var TILE_SPACING_FRACTION = 1 / 8;

  // The smallest amount of horizontal blank space to display on the sides when
  // displaying a wide arrangement.
  var MIN_WIDE_MARGIN = 100;

  /**
   * Creates a new TilePage object. This object contains tiles and controls
   * their layout.
   * @param {string} name The display name for the page.
   * @param {Object} gridValues Pixel values that define the size and layout
   *     of the tile grid.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function TilePage(name, gridValues) {
    var el = cr.doc.createElement('div');
    el.pageName = name;
    el.gridValues_ = gridValues;
    el.__proto__ = TilePage.prototype;
    el.initialize();

    return el;
  }

  /**
   * Takes a collection of grid layout pixel values and updates them with
   * additional tiling values that are calculated from TilePage constants.
   * @param {Object} grid The grid layout pixel values to update.
   */
  TilePage.initGridValues = function(grid) {
    // The amount of space we need to display a narrow grid (all narrow grids
    // are this size).
    grid.narrowWidth =
        grid.minTileWidth * tileWidthFraction(grid.minColCount);
    // The minimum amount of space we need to display a wide grid.
    grid.minWideWidth =
        grid.minTileWidth * tileWidthFraction(grid.maxColCount);
    // The largest we will ever display a wide grid.
    grid.maxWideWidth =
        grid.maxTileWidth * tileWidthFraction(grid.maxColCount);
    // Tile-related pixel values for the narrow display.
    grid.narrowTileValues = tileValuesForGrid(grid.narrowWidth,
                                              grid.minColCount);
    // Tile-related pixel values for the minimum narrow display.
    grid.wideTileValues = tileValuesForGrid(grid.minWideWidth,
                                            grid.maxColCount);
  },

  TilePage.prototype = {
    __proto__: HTMLDivElement.prototype,

    initialize: function() {
      this.className = 'tile-page';

      var title = this.ownerDocument.createElement('span');
      title.textContent = this.pageName;
      title.className = 'tile-page-title';
      this.appendChild(title);

      // Div that holds the tiles.
      this.tileGrid_ = this.ownerDocument.createElement('div');
      this.tileGrid_.className = 'tile-grid';
      this.appendChild(this.tileGrid_);

      // Ordered list of our tiles.
      this.tileElements_ = this.tileGrid_.getElementsByClassName('tile');

      this.lastWidth_ = this.clientWidth;

      this.eventTracker = new EventTracker();
      this.eventTracker.add(window, 'resize', this.onResize_.bind(this));
    },

    /**
     * Cleans up resources that are no longer needed after this TilePage
     * instance is removed from the DOM.
     */
    tearDown: function() {
      this.eventTracker.removeAll();
    },

    /**
     * @protected
     */
    appendTile: function(tileElement) {
      var wrapperDiv = tileElement.ownerDocument.createElement('div');
      wrapperDiv.appendChild(tileElement);
      wrapperDiv.className = 'tile';
      this.tileGrid_.appendChild(wrapperDiv);

      this.positionTile_(this.tileElements_.length - 1);
      this.classList.remove('resizing-tile-page');
    },

    /**
     * Calculates the x/y coordinates for an element and moves it there.
     * @param {number} The index of the element to be positioned.
     * @private
     */
    positionTile_: function(index) {
      var grid = this.gridValues_;

      var availableSpace = this.tileGrid_.clientWidth - 2 * MIN_WIDE_MARGIN;
      var wide = availableSpace >= grid.minWideWidth;
      // Calculate the portion of the tile's position that should be animated.
      var animatedTileValues = wide ?
          grid.wideTileValues : grid.narrowTileValues;
      // Animate the difference between three-wide and six-wide.
      var animatedLeftMargin = wide ?
          0 : (grid.minWideWidth - MIN_WIDE_MARGIN - grid.narrowWidth) / 2;

      var numRowTiles = wide ? grid.maxColCount : grid.minColCount;
      var col = index % numRowTiles;
      var row = Math.floor(index / numRowTiles);
      var animatedX = col * animatedTileValues.offsetX + animatedLeftMargin;
      var animatedY = row * (this.heightForWidth(animatedTileValues.tileWidth) +
                             animatedTileValues.interTileSpacing);

      // Calculate the final on-screen position for the tile.
      var effectiveGridWidth = wide ?
          Math.min(Math.max(availableSpace, grid.minWideWidth),
                   grid.maxWideWidth) :
          grid.narrowWidth;
      var realTileValues = tileValuesForGrid(effectiveGridWidth, numRowTiles);
      // leftMargin centers the grid within the avaiable space.
      var minMargin = wide ? MIN_WIDE_MARGIN : 0;
      var leftMargin =
          Math.max(minMargin,
                   (this.tileGrid_.clientWidth - effectiveGridWidth) / 2);
      var realX = col * realTileValues.offsetX + leftMargin;
      var realY = row * (this.heightForWidth(realTileValues.tileWidth) +
                         realTileValues.interTileSpacing);

      var tileWrapper = this.tileElements_[index];
      tileWrapper.style.left = animatedX + 'px';
      tileWrapper.style.top = animatedY + 'px';
      tileWrapper.firstChild.setBounds(realTileValues.tileWidth,
                                       realX - animatedX, realY - animatedY);
    },

    /**
     * Window resize event handler. Window resizes may trigger re-layouts.
     * @param {Object} e The resize event.
     */
    onResize_: function(e) {
      // Do nothing if the width didn't change.
      if (this.lastWidth_ == this.clientWidth)
        return;

      this.lastWidth_ = this.clientWidth;
      this.classList.add('resizing-tile-page');

      for (var i = 0; i < this.tileElements_.length; i++) {
        this.positionTile_(i);
      }
    },

    /**
     * Get the height for a tile of a certain width. Override this function to
     * get non-square tiles.
     * @param {number} width The pixel width of a tile.
     * @return {number} The height for |width|.
     */
    heightForWidth: function(width) {
      return width;
    },
  };

  return {
    TilePage: TilePage,
  };
});
