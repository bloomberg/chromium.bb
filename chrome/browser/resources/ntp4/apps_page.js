// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {
  'use strict';

  /**
   * Creates a new App object.
   * @param {Object} appData The data object that describes the app.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function App(appData) {
    var el = cr.doc.createElement('div');
    el.__proto__ = App.prototype;
    el.appData = appData;
    el.initialize();

    return el;
  }

  App.prototype = {
    __proto__: HTMLDivElement.prototype,

    initialize: function() {
      assert(this.appData.id, 'Got an app without an ID');

      this.className = 'app';
      this.setAttribute('app-id', this.appData.id);

      // The subcontainer wraps the icon and text. Its main purpose is to give
      // us additional, animated left/right CSS values (see setBounds).
      this.subcontainer_ = this.ownerDocument.createElement('div');
      this.subcontainer_.className = 'app-subcontainer';
      this.appendChild(this.subcontainer_);

      var appImg = this.ownerDocument.createElement('img');
      appImg.src = this.appData.icon_big;
      // We use a mask of the same image so CSS rules can highlight just the
      // image when it's touched.
      appImg.style.WebkitMaskImage = url(this.appData.icon_big);
      // We put a click handler just on the app image - so clicking on the
      // margins between apps doesn't do anything.
      appImg.addEventListener('click', this.onClick_.bind(this));
      this.subcontainer_.appendChild(appImg);
      this.appImg_ = appImg;

      var appSpan = this.ownerDocument.createElement('span');
      appSpan.textContent = this.appData.name;
      this.subcontainer_.appendChild(appSpan);

      /* TODO(estade): grabber */
    },

    /**
     * Set the size and position of the app tile. Part of the location should
     * animate (animatedX/Y) and the rest (realX/Y) should take effect
     * immediately.
     * @param {number} innerSize The size of the app icon.
     * @param {number} outerSize The total size of |this|.
     * @param {number} realX The actual x-position.
     * @param {number} realY The actual y-position.
     * @param {number} animatedX The portion of the x-position that should
     *     animate.
     * @param {number} animatedY The portion of the y-position that should
     *     animate.
     */
    setBounds: function(innerSize, outerSize,
                        realX, realY,
                        animatedX, animatedY) {
      this.appImg_.style.width = this.appImg_.style.height = innerSize + 'px';
      this.subcontainer_.style.width = this.subcontainer_.style.height =
          outerSize + 'px';
      this.subcontainer_.style.left = animatedX + 'px';
      this.subcontainer_.style.top = animatedY + 'px';
      this.style.left = (realX - animatedX) + 'px';
      this.style.top = (realY - animatedY) + 'px';
    },

    /**
     * Invoked when an app is clicked
     * @param {Event} e The click event.
     * @private
     */
    onClick_: function(e) {
      // Tell chrome to launch the app.
      var NTP_APPS_MAXIMIZED = 0;
      chrome.send('launchApp', [this.appData.id, NTP_APPS_MAXIMIZED]);

      // Don't allow the click to trigger a link or anything
      e.preventDefault();
    },
  };

  /**
   * Gives the proportion of the row width that is devoted to a single icon.
   * @param {number} rowTileCount The number of tiles in a row.
   * @return {number} The ratio between icon width and row width.
   */
  function iconWidthFraction(rowTileCount) {
    return rowTileCount +
        rowTileCount * 2 * ICON_PADDING_FRACTION +
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
    var iconWidth = Math.min(MAX_ICON_WIDTH,
        width / iconWidthFraction(numRowTiles));
    var tileWidth = iconWidth + iconWidth * 2 * ICON_PADDING_FRACTION;
    var offset = tileWidth + iconWidth * TILE_SPACING_FRACTION;

    return {
      iconWidth: iconWidth,
      tileWidth: tileWidth,
      offset: offset,
    };
  }

  // The most tiles we will show in a row.
  var MAX_ROW_TILE_COUNT = 6;
  // The fewest tiles we will show in a row.
  var MIN_ROW_TILE_COUNT = 3;

  // The smallest an app icon will ever get.
  var MIN_ICON_WIDTH = 96;
  // The largest an app icon can be.
  var MAX_ICON_WIDTH = 128;

  // The proportion of the icon width which will be added to each side of the
  // icon as padding (within the tile).
  var ICON_PADDING_FRACTION = 1 / 12;
  // The proportion of the icon width which will be used as spacing between
  // tiles.
  var TILE_SPACING_FRACTION = 1 / 6;

  // The smallest amount of horizontal blank space to display on the sides when
  // displaying a six-up arrangement.
  var MIN_SIXUP_MARGIN = 100;

  // The minimum amount of space we need to display a six-wide grid.
  var MIN_SIXUP_WIDTH = MIN_ICON_WIDTH * iconWidthFraction(MAX_ROW_TILE_COUNT);
  // The largest we will ever display a six-wide grid.
  var MAX_SIXUP_WIDTH = MAX_ICON_WIDTH * iconWidthFraction(MAX_ROW_TILE_COUNT);
  // The amount of space we need to display a three-wide grid (all
  // three-wide grids are this size).
  var THREEUP_WIDTH = MIN_ICON_WIDTH * iconWidthFraction(MIN_ROW_TILE_COUNT);

  // Tile-related pixel values for the minimum six-wide display.
  var LARGE_TILE_VALUES =
      tileValuesForGrid(MIN_SIXUP_WIDTH, MAX_ROW_TILE_COUNT);
  // Tile-related pixel values for the three-wide display.
  var SMALL_TILE_VALUES = tileValuesForGrid(THREEUP_WIDTH, MIN_ROW_TILE_COUNT);

  /**
   * Creates a new AppsPage object. This object contains apps and controls
   * their layout.
   * @param {string} name The display name for the page.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function AppsPage(name) {
    var el = cr.doc.createElement('div');
    el.pageName = name;
    el.__proto__ = AppsPage.prototype;
    el.initialize();

    return el;
  }

  AppsPage.prototype = {
    __proto__: HTMLDivElement.prototype,

    initialize: function() {
      this.className = 'apps-page';

      var title = this.ownerDocument.createElement('span');
      title.textContent = this.pageName;
      title.className = 'apps-page-title';
      this.appendChild(title);

      // Div that holds the apps.
      this.tileGrid_ = this.ownerDocument.createElement('div');
      this.tileGrid_.className = 'tile-grid';
      this.appendChild(this.tileGrid_);

      // Ordered list of our apps.
      this.appElements = this.tileGrid_.getElementsByClassName('app');

      this.lastWidth_ = this.clientWidth;

      this.eventTracker = new EventTracker();
      this.eventTracker.add(window, 'resize', this.onResize_.bind(this));
    },

    /**
     * Cleans up resources that are no longer needed after this AppsPage
     * instance is removed from the DOM.
     */
    tearDown: function() {
      this.eventTracker.removeAll();
    },

    /**
     * Creates an app DOM element and places it at the last position on the
     * page.
     * TODO(estade): don't animate initial apps.
     * @param {Object} appData The data object that describes the app.
     */
    appendApp: function(appData) {
      var appElement = new App(appData);
      this.tileGrid_.appendChild(appElement);

      this.positionApp_(this.appElements.length - 1);
      this.classList.remove('resizing-apps-page');
    },

    /**
     * Calculates the x/y coordinates for an element and moves it there.
     * @param {number} The index of the element to be positioned.
     */
    positionApp_: function(index) {
      var availableSpace = this.tileGrid_.clientWidth - 2 * MIN_SIXUP_MARGIN;
      var numRowTiles = availableSpace < MIN_SIXUP_WIDTH ?
          MIN_ROW_TILE_COUNT : MAX_ROW_TILE_COUNT;

      var col = index % numRowTiles;
      var row = Math.floor(index / numRowTiles);

      // Calculate the portion of the tile's position that should be animated.
      var animatedTileValues = numRowTiles == MAX_ROW_TILE_COUNT ?
          LARGE_TILE_VALUES : SMALL_TILE_VALUES;
      // Animate the difference between three-wide and six-wide.
      var animatedLeftMargin = numRowTiles == MAX_ROW_TILE_COUNT ?
          0 : (MIN_SIXUP_WIDTH - MIN_SIXUP_MARGIN - THREEUP_WIDTH) / 2;
      var animatedX = col * animatedTileValues.offset + animatedLeftMargin;
      var animatedY = row * animatedTileValues.offset;

      // Calculate the final on-screen position for the tile.
      var effectiveGridWidth = THREEUP_WIDTH;
      if (numRowTiles == MAX_ROW_TILE_COUNT) {
        effectiveGridWidth =
            Math.min(Math.max(availableSpace, MIN_SIXUP_WIDTH),
                     MAX_SIXUP_WIDTH);
      }
      var realTileValues = tileValuesForGrid(effectiveGridWidth, numRowTiles);
      // leftMargin centers the grid within the avaiable space.
      var minMargin = numRowTiles == MAX_ROW_TILE_COUNT ? MIN_SIXUP_MARGIN : 0;
      var leftMargin =
          Math.max(minMargin,
                   (this.tileGrid_.clientWidth - effectiveGridWidth) / 2);
      var realX = col * realTileValues.offset + leftMargin;
      var realY = row * realTileValues.offset;

      this.appElements[index].setBounds(
          realTileValues.iconWidth, realTileValues.tileWidth,
          realX, realY, animatedX, animatedY);
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
      this.classList.add('resizing-apps-page');

      for (var i = 0; i < this.appElements.length; i++) {
        this.positionApp_(i);
      }
    },
  };

  return {
    AppsPage: AppsPage,
  };
});
