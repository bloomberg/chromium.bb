// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp', function() {
  'use strict';

  /**
   * The maximum gap from the edge of the scrolling area which will display
   * the shadow with transparency. After this point the shadow will become
   * 100% opaque.
   * @type {number}
   * @const
   */
  var MAX_SCROLL_SHADOW_GAP = 16;

  /**
   * @type {number}
   * @const
   */
  var SCROLL_BAR_WIDTH = 12;

  //----------------------------------------------------------------------------
  // Tile
  //----------------------------------------------------------------------------

  /**
   * A virtual Tile class. Each TilePage subclass should have its own Tile
   * subclass implemented too (e.g. MostVisitedPage contains MostVisited
   * tiles, and MostVisited is a Tile subclass).
   * @constructor
   * @param {Object} config TilePage configuration object.
   */
  function Tile(config) {
    console.error('Tile is a virtual class and is not supposed to be ' +
        'instantiated');
  }

  /**
   * Creates a Tile subclass. We need to use this function to create a Tile
   * subclass because a Tile must also subclass a HTMLElement (which can be
   * any HTMLElement), so we need to individually add methods and getters here.
   * @param {Object} Subclass The prototype object of the class we want to be
   *     a Tile subclass.
   * @param {Object} The extended Subclass object.
   */
  Tile.subclass = function(Subclass) {
    var Base = Tile.prototype;
    for (var name in Base) {
      if (!Subclass.hasOwnProperty(name))
        Subclass[name] = Base[name];
    }
    for (var name in TileGetters) {
      if (!Subclass.hasOwnProperty(name))
        Subclass.__defineGetter__(name, TileGetters[name]);
    }
    return Subclass;
  };

  Tile.prototype = {
    // Tile data object.
    data_: null,

    /**
     * Initializes a Tile.
     * @param {Object} config TilePage configuration object.
     */
    initialize: function(config) {
      this.classList.add('tile');
      this.reset();
    },

    /**
     * Resets the tile DOM.
     */
    reset: function() {
    },

    /**
     * Update the appearance of this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     */
    setData: function(data) {
      // TODO(pedrosimonetti): Remove data.filler usage everywhere.
      if (!data || data.filler) {
        if (this.data_)
          this.reset();
        return;
      }

      this.data_ = data;
    }
  };

  var TileGetters = {
    /**
     * The TileCell associated to this Tile.
     * @type {TileCell}
     */
    'tileCell': function() {
      return findAncestorByClass(this, 'tile-cell');
    },

    /**
     * The index of the Tile.
     * @type {number}
     */
    'index': function() {
      assert(this.tileCell);
      return this.tileCell.index;
    },

    /**
     * Tile data object.
     * @type {Object}
     */
    'data': function() {
      return this.data_;
    }
  };

  //----------------------------------------------------------------------------
  // TileCell
  //----------------------------------------------------------------------------

  /**
   * Creates a new TileCell object. A TileCell represents a cell in the
   * TilePage's grid. A TilePage uses TileCells to position Tiles in the proper
   * place and to animate them individually. Each TileCell is associated to
   * one Tile at a time (or none if it is a filler object), and that association
   * might change when the grid is resized. When that happens, the grid is
   * updated and the Tiles are moved to the proper TileCell. We cannot move the
   * the TileCell itself during the resize because this transition is animated
   * with CSS and there's no way to stop CSS animations, and we really want to
   * animate with CSS to take advantage of hardware acceleration.
   * @constructor
   * @extends {HTMLDivElement}
   * @param {HTMLElement} tile Tile element that will be associated to the cell.
   * @param {Object} config TilePage configuration object.
   */
  function TileCell(tile, config) {
    var tileCell = cr.doc.createElement('div');
    tileCell.__proto__ = TileCell.prototype;
    tileCell.initialize(tile, config);

    return tileCell;
  }

  TileCell.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Initializes a TileCell.
     * @param {Tile} tile The Tile that will be assigned to this TileCell.
     * @param {Object} config TilePage configuration object.
     */
    initialize: function(tile, config) {
      this.className = 'tile-cell';
      this.assign(tile);
    },

    /**
     * The index of the TileCell.
     * @type {number}
     */
    get index() {
      return Array.prototype.indexOf.call(this.tilePage.tiles_,
          this.firstChild);
    },

    /**
     * The TilePage associated to this TileCell.
     * @type {TilePage}
     */
    get tilePage() {
      return findAncestorByClass(this, 'tile-page');
    },

    /**
     * Assigns a Tile to the this TileCell.
     * @type {TilePage}
     */
    assign: function(tile) {
      if (this.firstChild)
        this.replaceChild(tile, this.firstChild);
      else
        this.appendChild(tile);
    },

    /**
     * Called when an app is removed from Chrome. Animates its disappearance.
     * @param {boolean=} opt_animate Whether the animation should be animated.
     */
    doRemove: function(opt_animate) {
      if (opt_animate)
        this.firstChild.classList.add('removing-tile-contents');
      else
        this.tilePage.removeTile(this, false);
    },
  };

  //----------------------------------------------------------------------------
  // TilePage
  //----------------------------------------------------------------------------

  /**
   * Creates a new TilePage object. This object contains tiles and controls
   * their layout.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function TilePage() {
    var el = cr.doc.createElement('div');
    el.__proto__ = TilePage.prototype;

    return el;
  }

  TilePage.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Reference to the Tile subclass that will be used to create the tiles.
     * @constructor
     * @extends {Tile}
     */
    TileClass: Tile,

    // The config object should be defined by a TilePage subclass if it
    // wants the non-default behavior.
    config: {
      // The width of a cell.
      cellWidth: 110,
      // The start margin of a cell (left or right according to text direction).
      cellMarginStart: 12,
      // The maximum number of Tiles to be displayed.
      maxTileCount: 6,
      // Whether the TilePage content will be scrollable.
      scrollable: false,
    },

    /**
     * Initializes a TilePage.
     */
    initialize: function() {
      this.className = 'tile-page';

      // The div that wraps the scrollable element.
      this.frame_ = this.ownerDocument.createElement('div');
      this.frame_.className = 'tile-page-frame';
      this.appendChild(this.frame_);

      // The content/scrollable element.
      this.content_ = this.ownerDocument.createElement('div');
      this.content_.className = 'tile-page-content';
      this.frame_.appendChild(this.content_);

      if (this.config.scrollable) {
        this.content_.classList.add('scrollable');

        // The scrollable shadow top.
        this.shadowTop_ = this.ownerDocument.createElement('div');
        this.shadowTop_.className = 'shadow-top';
        this.content_.appendChild(this.shadowTop_);

        // The scrollable shadow bottom.
        this.shadowBottom_ = this.ownerDocument.createElement('div');
        this.shadowBottom_.className = 'shadow-bottom';
        this.content_.appendChild(this.shadowBottom_);
      }

      // The div that defines the tile grid viewport.
      this.tileGrid_ = this.ownerDocument.createElement('div');
      this.tileGrid_.className = 'tile-grid';
      this.content_.appendChild(this.tileGrid_);

      // The tile grid contents, which can be scrolled.
      this.tileGridContent_ = this.ownerDocument.createElement('div');
      this.tileGridContent_.className = 'tile-grid-content';
      this.tileGrid_.appendChild(this.tileGridContent_);

      // The list of Tile elements which is used to fill the TileGrid cells.
      this.tiles_ = [];

      // TODO(pedrosimonetti): Check duplication of these methods.
      this.addEventListener('cardselected', this.handleCardSelection_);
      this.addEventListener('carddeselected', this.handleCardDeselection_);

      this.tileGrid_.addEventListener('webkitTransitionEnd',
          this.onTileGridTransitionEnd_.bind(this));

      this.content_.addEventListener('scroll', this.onScroll.bind(this));
    },

    /**
     * The list of Tile elements.
     * @type {Array<Tile>}
     */
    get tiles() {
      return this.tiles_;
    },

    /**
     * The number of Tiles in this TilePage.
     * @type {number}
     */
    get tileCount() {
      return this.tiles_.length;
    },

    /**
     * Whether or not this TilePage is selected.
     * @type {boolean}
     */
    get selected() {
      return Array.prototype.indexOf.call(this.parentNode.children, this) ==
          ntp.getCardSlider().currentCard;
    },

    /**
     * Removes the tilePage from the DOM and cleans up event handlers.
     */
    remove: function() {
      // This checks arguments.length as most remove functions have a boolean
      // |opt_animate| argument, but that's not necesarilly applicable to
      // removing a tilePage. Selecting a different card in an animated way and
      // deleting the card afterward is probably a better choice.
      assert(typeof arguments[0] != 'boolean',
             'This function takes no |opt_animate| argument.');
      this.parentNode.removeChild(this);
    },

    /**
     * Notify interested subscribers that a tile has been removed from this
     * page. TODO(pedrosimonetti): Do we really need to fire this event?
     * @param {TileCell} tile The newly added tile.
     * @param {number} index The index of the tile that was added.
     * @param {boolean} wasAnimated Whether the removal was animated.
     */
    fireAddedEvent: function(tile, index, wasAnimated) {
      var e = document.createEvent('Event');
      e.initEvent('tilePage:tile_added', true, true);
      e.addedIndex = index;
      e.addedTile = tile;
      e.wasAnimated = wasAnimated;
      this.dispatchEvent(e);
    },

    /**
     * Removes the given tile and animates the repositioning of the other tiles.
     * @param {boolean=} opt_animate Whether the removal should be animated.
     * @param {boolean=} opt_dontNotify Whether a page should be removed if the
     *     last tile is removed from it.
     */
    removeTile: function(tile, opt_animate, opt_dontNotify) {
      var tiles = this.tiles;
      var index = tiles.indexOf(tile);
      tile.parentNode.removeChild(tile);
      tiles.splice(index, 1);
      this.renderGrid_();

      if (!opt_dontNotify)
        this.fireRemovedEvent(tile, index, !!opt_animate);
    },

    /**
     * Notify interested subscribers that a tile has been removed from this
     * page.
     * @param {TileCell} tile The tile that was removed.
     * @param {number} oldIndex Where the tile was positioned before removal.
     * @param {boolean} wasAnimated Whether the removal was animated.
     */
    fireRemovedEvent: function(tile, oldIndex, wasAnimated) {
      var e = document.createEvent('Event');
      e.initEvent('tilePage:tile_removed', true, true);
      e.removedIndex = oldIndex;
      e.removedTile = tile;
      e.wasAnimated = wasAnimated;
      this.dispatchEvent(e);
    },

    /**
     * Removes all tiles from the page.
     */
    removeAllTiles: function() {
      this.tileGrid_.innerHTML = '';
    },

    /**
     * Called when the page is selected (in the card selector).
     * @param {Event} e A custom cardselected event.
     * @private
     */
    handleCardSelection_: function(e) {
      ntp.layout();
    },

    /**
     * Called when the page loses selection (in the card selector).
     * @param {Event} e A custom carddeselected event.
     * @private
     */
    handleCardDeselection_: function(e) {
    },

    // #########################################################################
    // Extended Chrome Instant
    // #########################################################################


    // properties
    // -------------------------------------------------------------------------

    // The number of columns.
    colCount_: 0,
    // The number of rows.
    rowCount_: 0,
    // The number of visible rows. We initialize this value with zero so
    // we can detect when the first time the page is rendered.
    numOfVisibleRows_: 1,
    // The number of the last column being animated. We initialize this value
    // with zero so we can detect when the first time the page is rendered.
    animatingColCount_: 0,
    // The index of the topmost row visible.
    pageOffset_: 0,
    // Data object representing the tiles.
    dataList_: null,

    /**
     * Appends a tile to the end of the tile grid.
     * @param {Tile} tile The tile to be added.
     * @param {number} index The location in the tile grid to insert it at.
     * @protected
     */
    appendTile: function(tile) {
      var index = this.tiles_.length;
      this.addTileAt(tile, index);
    },

    /**
     * Adds the given element to the tile grid.
     * @param {Tile} tile The tile to be added.
     * @param {number} index The location in the tile grid to insert it at.
     * @protected
     */
    addTileAt: function(tile, index) {
      this.tiles_.splice(index, 0, tile);
      this.fireAddedEvent(tile, index);
    },

    /**
     * Create blank tiles.
     * @private
     * @param {number} count The number of Tiles to be created.
     */
    createTiles_: function(count) {
      var Class = this.TileClass;
      var config = this.config;
      count = Math.min(count, config.maxTileCount);
      for (var i = 0; i < count; i++) {
        this.appendTile(new Class(config));
      }
    },

    /**
     * Update the tiles after a change to |dataList_|.
     */
    updateTiles_: function() {
      var maxTileCount = this.config.maxTileCount;
      var dataList = this.dataList_;
      var tiles = this.tiles;
      for (var i = 0; i < maxTileCount; i++) {
        var data = dataList[i];
        var tile = tiles[i];

        // TODO(pedrosimonetti): What do we do when there's no tile here?
        if (!tile)
          return;

        if (i >= dataList.length)
          tile.reset();
        else
          tile.setData(data);
      }
    },

    /**
     * Sets the dataList that will be used to create Tiles. This method will
     * create/remove necessary/unnecessary tiles, render the grid when the
     * number of tiles has changed, and finally will call |updateTiles_| which
     * will in turn render the individual tiles.
     * @param {Array} dataList The array of data.
     */
    setDataList: function(dataList) {
      this.dataList_ = dataList.slice(0, this.config.maxTileCount);

      var dataListLength = this.dataList_.length;
      var tileCount = this.tileCount;
      // Create or remove tiles if necessary.
      if (tileCount < dataListLength) {
        this.createTiles_(dataListLength - tileCount);
      } else if (tileCount > dataListLength) {
        var tiles = this.tiles_;
        while (tiles.length > dataListLength) {
          var previousLength = tiles.length;
          // It doesn't matter which tiles are being removed here because
          // they're going to be reconstructed below when calling updateTiles_
          // method, so the first tiles are being removed here.
          this.removeTile(tiles[0]);
          assert(tiles.length < previousLength);
        }
      }

      if (dataListLength != tileCount)
        this.renderGrid_();

      this.updateTiles_();
    },

    // internal helpers
    // -------------------------------------------------------------------------

    /**
     * Gets the required width for a Tile.
     * @private
     */
    getTileRequiredWidth_: function() {
      var config = this.config;
      return config.cellWidth + config.cellMarginStart;
    },

    /**
     * Gets the the maximum number of columns that can fit in a given width.
     * @param {number} width The width in pixels.
     * @private
     */
    getColCountForWidth_: function(width) {
      var scrollBarIsVisible = this.config.scrollable &&
          this.content_.scrollHeight > this.content_.clientHeight;
      var scrollBarWidth = scrollBarIsVisible ? SCROLL_BAR_WIDTH : 0;
      var availableWidth = width + this.config.cellMarginStart - scrollBarWidth;

      var requiredWidth = this.getTileRequiredWidth_();
      var colCount = Math.floor(availableWidth / requiredWidth);
      return colCount;
    },

    /**
     * Gets the width for a given number of columns.
     * @param {number} colCount The number of columns.
     * @private
     */
    getWidthForColCount_: function(colCount) {
      var requiredWidth = this.getTileRequiredWidth_();
      var width = colCount * requiredWidth - this.config.cellMarginStart;
      return width;
    },

    /**
     * Returns the position of the tile at |index|.
     * @param {number} index Tile index.
     * @private
     * @return {!{top: number, left: number}} Position.
     */
    getTilePosition_: function(index) {
      var colCount = this.colCount_;
      var row = Math.floor(index / colCount);
      var col = index % colCount;
      if (isRTL())
        col = colCount - col - 1;
      var config = this.config;
      var top = ntp.TILE_ROW_HEIGHT * row;
      var left = col * (config.cellWidth + config.cellMarginStart);
      return {top: top, left: left};
    },

    // rendering
    // -------------------------------------------------------------------------

    /**
     * Renders the tile grid, and the individual tiles. Rendering the grid
     * consists of adding/removing tile rows and tile cells according to the
     * specified size (defined by the number of columns in the grid). While
     * rendering the grid, the tiles are rendered in order in their respective
     * cells and tile fillers are rendered when needed. This method sets the
     * private properties colCount_ and rowCount_.
     *
     * This method should be called every time the contents of the grid changes,
     * that is, when the number, contents or order of the tiles has changed.
     * @param {number=} opt_colCount The number of columns.
     * @private
     */
    renderGrid_: function(opt_colCount) {
      var colCount = opt_colCount || this.colCount_;

      var tileGridContent = this.tileGridContent_;
      var tiles = this.tiles_;
      var tileCount = tiles.length;

      var numOfVisibleRows = this.numOfVisibleRows_;
      var rowCount = Math.ceil(tileCount / colCount);
      var tileRows = tileGridContent.getElementsByClassName('tile-row');

      var pageOffset = this.pageOffset_;

      for (var tile = 0, row = 0; row < rowCount; row++) {
        var tileRow = tileRows[row];

        // Create tile row if there's no one yet.
        if (!tileRow) {
          tileRow = cr.doc.createElement('div');
          tileRow.className = 'tile-row';
          tileGridContent.appendChild(tileRow);
        }

        // The tiles inside the current row.
        var tileRowTiles = tileRow.childNodes;

        // Remove excessive columns from a particular tile row.
        var maxColCount = Math.min(colCount, tileCount - tile);
        maxColCount = Math.max(0, maxColCount);
        while (tileRowTiles.length > maxColCount) {
          tileRow.removeChild(tileRow.lastElementChild);
        }

        // For each column in the current row.
        for (var col = 0; col < colCount; col++, tile++) {
          var tileCell;
          var tileElement;
          if (tileRowTiles[col]) {
            tileCell = tileRowTiles[col];
          } else {
            var span = cr.doc.createElement('span');
            tileCell = new TileCell(span, this.config);
          }

          // Render Tiles.
          if (tile < tileCount) {
            tileCell.classList.remove('filler');
            tileElement = tiles[tile];
            if (!tileCell.firstChild)
              tileCell.appendChild(tileElement);
            else if (tileElement != tileCell.firstChild)
              tileCell.replaceChild(tileElement, tileCell.firstChild);
          } else if (!tileCell.classList.contains('filler')) {
            tileCell.classList.add('filler');
            tileElement = cr.doc.createElement('span');
            tileElement.className = 'tile';
            if (tileCell.firstChild)
              tileCell.replaceChild(tileElement, tileCell.firstChild);
            else
              tileCell.appendChild(tileElement);
          }

          if (!tileRowTiles[col])
            tileRow.appendChild(tileCell);
        }
      }

      // Remove excessive tile rows from the tile grid.
      while (tileRows.length > rowCount) {
        tileGridContent.removeChild(tileGridContent.lastElementChild);
      }

      this.colCount_ = colCount;
      this.rowCount_ = rowCount;

      this.onScroll();
    },

    // layout
    // -------------------------------------------------------------------------

    /**
     * Calculates the layout of the tile page according to the current Bottom
     * Panel's size. This method will resize the containers of the tile page,
     * and re-render the grid when its dimension changes (number of columns or
     * visible rows changes). This method also sets the private properties
     * |numOfVisibleRows_| and |animatingColCount_|.
     *
     * This method should be called every time the dimension of the grid changes
     * or when you need to reinforce its dimension.
     * @param {boolean=} opt_animate Whether the layout be animated.
     */
    layout: function(opt_animate) {
      var contentHeight = ntp.getContentHeight();
      this.content_.style.height = contentHeight + 'px';

      var contentWidth = ntp.getContentWidth();
      var colCount = this.getColCountForWidth_(contentWidth);
      var lastColCount = this.colCount_;
      var animatingColCount = this.animatingColCount_;
      if (colCount != animatingColCount) {
        if (opt_animate)
          this.tileGrid_.classList.add('animate-grid-width');

        if (colCount > animatingColCount) {
          // If the grid is expanding, it needs to be rendered first so the
          // revealing tiles are visible as soon as the animation starts.
          if (colCount != lastColCount)
            this.renderGrid_(colCount);

          // Hides affected columns and forces the reflow.
          this.showTileCols_(animatingColCount, false);
          // Trigger reflow, making the tiles completely hidden.
          this.tileGrid_.offsetTop;
          // Fades in the affected columns.
          this.showTileCols_(animatingColCount, true);
        } else {
          // Fades out the affected columns.
          this.showTileCols_(colCount, false);
        }

        var newWidth = this.getWidthForColCount_(colCount);
        this.tileGrid_.style.width = newWidth + 'px';

        // TODO(pedrosimonetti): move to handler below.
        var self = this;
        this.onTileGridTransitionEndHandler_ = function() {
          if (colCount < lastColCount)
            self.renderGrid_(colCount);
          else
            self.showTileCols_(0, true);
        };
      }

      this.animatingColCount_ = colCount;

      this.frame_.style.width = contentWidth + 'px';

      this.onScroll();
    },

    // tile repositioning animation
    // -------------------------------------------------------------------------

    /**
     * Tile repositioning state.
     * @type {{index: number, isRemoving: number}}
     */
    tileRepositioningState_: null,

    /**
     * Gets the repositioning state.
     * @return {{index: number, isRemoving: number}} The repositioning data.
     */
    getTileRepositioningState: function() {
      return this.tileRepositioningState_;
    },

    /**
     * Sets the repositioning state that will be used to animate the tiles.
     * @param {number} index The tile's index.
     * @param {boolean} isRemoving Whether the tile is being removed.
     */
    setTileRepositioningState: function(index, isRemoving) {
      this.tileRepositioningState_ = {
        index: index,
        isRemoving: isRemoving
      };
    },

    /**
     * Resets the repositioning state.
     */
    resetTileRepositioningState: function() {
      this.tileRepositioningState_ = null;
    },

    /**
     * Animates a tile removal.
     * @param {number} index The index of the tile to be removed.
     * @param {Object} newDataList The new data list.
     */
    animateTileRemoval: function(index, newDataList) {
      var tiles = this.tiles_;
      var tileCount = tiles.length;
      assert(tileCount > 0);

      var tileCells = this.querySelectorAll('.tile-cell');
      var extraTileIndex = tileCount - 1;
      var extraCell = tileCells[extraTileIndex];
      var extraTileData = newDataList[extraTileIndex];

      var repositioningStartIndex = index + 1;
      var repositioningEndIndex = tileCount;

      this.initializeRepositioningAnimation_(index, repositioningEndIndex,
          true);

      var tileBeingRemoved = tiles[index];
      tileBeingRemoved.scrollTop;

      // The extra tile is the new one that will appear. It can be a normal
      // tile (when there's extra data for it), or a filler tile.
      var extraTile = createTile(this, extraTileData);
      if (!extraTileData)
        extraCell.classList.add('filler');
      // The extra tile is being assigned in order to put it in the right spot.
      extraCell.assign(extraTile);

      this.executeRepositioningAnimation_(tileBeingRemoved, extraTile,
          repositioningStartIndex, repositioningEndIndex, true);

      // Cleans up the animation.
      var onPositioningTransitionEnd = function(e) {
        var propertyName = e.propertyName;
        if (!(propertyName == '-webkit-transform' ||
              propertyName == 'opacity')) {
          return;
        }

        lastAnimatingTile.removeEventListener('webkitTransitionEnd',
            onPositioningTransitionEnd);

        this.finalizeRepositioningAnimation_(tileBeingRemoved,
            repositioningStartIndex, repositioningEndIndex, true);

        this.removeTile(tileBeingRemoved);

        // If the extra tile is a real one (not a filler), then it needs to be
        // added to the tile list. The tile has been placed in the right spot
        // but the tile page still doesn't know about this new tile.
        if (extraTileData)
          this.appendTile(extraTile);

        this.renderGrid_();
      }.bind(this);

      // Listens to the animation end.
      var lastAnimatingTile = extraTile;
      lastAnimatingTile.addEventListener('webkitTransitionEnd',
          onPositioningTransitionEnd);
    },

    /**
     * Animates a tile restoration.
     * @param {number} index The index of the tile to be restored.
     * @param {Object} newDataList The new data list.
     */
    animateTileRestoration: function(index, newDataList) {
      var tiles = this.tiles_;
      var tileCount = tiles.length;

      var tileCells = this.querySelectorAll('.tile-cell');
      var extraTileIndex = Math.min(tileCount, this.config.maxTileCount - 1);
      var extraCell = tileCells[extraTileIndex];
      var extraTileData = newDataList[extraTileIndex + 1];

      var repositioningStartIndex = index;
      var repositioningEndIndex = tileCount - (extraTileData ? 1 : 0);

      this.initializeRepositioningAnimation_(index, repositioningEndIndex);

      var restoredData = newDataList[index];
      var tileBeingRestored = createTile(this, restoredData);
      tileCells[index].appendChild(tileBeingRestored);

      var extraTile = extraCell.firstChild;

      this.executeRepositioningAnimation_(tileBeingRestored, extraTile,
          repositioningStartIndex, repositioningEndIndex, false);

      // Cleans up the animation.
      var onPositioningTransitionEnd = function(e) {
        var propertyName = e.propertyName;
        if (!(propertyName == '-webkit-transform' ||
              propertyName == 'opacity')) {
          return;
        }

        lastAnimatingTile.removeEventListener('webkitTransitionEnd',
            onPositioningTransitionEnd);

        // When there's an extra data, it means the tile is a real one (not a
        // filler), and therefore it needs to be removed from the tile list.
        if (extraTileData)
          this.removeTile(extraTile);

        this.finalizeRepositioningAnimation_(tileBeingRestored,
            repositioningStartIndex, repositioningEndIndex, false);

        this.addTileAt(tileBeingRestored, index);

        this.renderGrid_();
      }.bind(this);

      // Listens to the animation end.
      var lastAnimatingTile = tileBeingRestored;
      lastAnimatingTile.addEventListener('webkitTransitionEnd',
          onPositioningTransitionEnd);
    },

    // animation helpers
    // -------------------------------------------------------------------------

    /**
     * Moves a tile to a new position.
     * @param {Tile} tile A tile.
     * @param {number} left Left coordinate.
     * @param {number} top Top coordinate.
     * @private
     */
    moveTileTo_: function(tile, left, top) {
      tile.style.left = left + 'px';
      tile.style.top = top + 'px';
    },

    /**
     * Resets a tile's position.
     * @param {Tile} tile A tile.
     * @private
     */
    resetTilePosition_: function(tile) {
      tile.style.left = '';
      tile.style.top = '';
    },

    /**
     * Initializes the repositioning animation.
     * @param {number} startIndex Index of the first tile to be repositioned.
     * @param {number} endIndex Index of the last tile to be repositioned.
     * @param {boolean} isRemoving Whether the tile is being removed.
     * @private
     */
    initializeRepositioningAnimation_: function(startIndex, endIndex,
        isRemoving) {
      // Move tiles from relative to absolute position.
      var tiles = this.tiles_;
      var tileGridContent = this.tileGridContent_;
      for (var i = startIndex; i < endIndex; i++) {
        var tile = tiles[i];
        var position = this.getTilePosition_(i);
        this.moveTileTo_(tile, position.left, position.top);
        tile.style.zIndex = endIndex - i;
        tileGridContent.appendChild(tile);
      }

      tileGridContent.classList.add('animate-tile-repositioning');

      if (!isRemoving)
        tileGridContent.classList.add('undo-removal');
    },

    /**
     * Executes the repositioning animation.
     * @param {Tile} targetTile The tile that is being removed/restored.
     * @param {Tile} extraTile The extra tile that is going to appear/disappear.
     * @param {number} startIndex Index of the first tile to be repositioned.
     * @param {number} endIndex Index of the last tile to be repositioned.
     * @param {boolean} isRemoving Whether the tile is being removed.
     * @private
     */
    executeRepositioningAnimation_: function(targetTile, extraTile, startIndex,
        endIndex, isRemoving) {
      targetTile.classList.add('target-tile');

      // Alternate the visualization of the target and extra tiles.
      fadeTile(targetTile, !isRemoving);
      fadeTile(extraTile, isRemoving);

      // Move tiles to the new position.
      var tiles = this.tiles_;
      var positionDiff = isRemoving ? -1 : 1;
      for (var i = startIndex; i < endIndex; i++) {
        var position = this.getTilePosition_(i + positionDiff);
        this.moveTileTo_(tiles[i], position.left, position.top);
      }
    },

    /**
     * Finalizes the repositioning animation.
     * @param {Tile} targetTile The tile that is being removed/restored.
     * @param {number} startIndex Index of the first tile to be repositioned.
     * @param {number} endIndex Index of the last tile to be repositioned.
     * @param {boolean} isRemoving Whether the tile is being removed.
     * @private
     */
    finalizeRepositioningAnimation_: function(targetTile, startIndex, endIndex,
        isRemoving) {
      // Remove temporary class names.
      var tileGridContent = this.tileGridContent_;
      tileGridContent.classList.remove('animate-tile-repositioning');
      tileGridContent.classList.remove('undo-removal');
      targetTile.classList.remove('target-tile');

      // Move tiles back to relative position.
      var tiles = this.tiles_;
      var tileCells = this.querySelectorAll('.tile-cell');
      var positionDiff = isRemoving ? -1 : 1;
      for (var i = startIndex; i < endIndex; i++) {
        var tile = tiles[i];
        this.resetTilePosition_(tile);
        tile.style.zIndex = '';
        tileCells[i + positionDiff].assign(tile);
      }
    },

    /**
     * Animates the display of columns.
     * @param {number} col The column number.
     * @param {boolean} show Whether or not to show the row.
     */
    showTileCols_: function(col, show) {
      var prop = show ? 'remove' : 'add';
      var max = 10;  // TODO(pedrosimonetti): Add const?
      var tileGridContent = this.tileGridContent_;
      for (var i = col; i < max; i++) {
        tileGridContent.classList[prop]('hide-col-' + i);
      }
    },

    // event handlers
    // -------------------------------------------------------------------------

    /**
     * Handles the scroll event.
     * @protected
     */
    onScroll: function() {
      // If the TilePage is scrollable, then the opacity of shadow top and
      // bottom must adjusted, indicating when there's an overflow content.
      if (this.config.scrollable) {
        var content = this.content_;
        var topGap = Math.min(MAX_SCROLL_SHADOW_GAP, content.scrollTop);
        var bottomGap = Math.min(MAX_SCROLL_SHADOW_GAP, content.scrollHeight -
            content.scrollTop - content.clientHeight);

        this.shadowTop_.style.opacity = topGap / MAX_SCROLL_SHADOW_GAP;
        this.shadowBottom_.style.opacity = bottomGap / MAX_SCROLL_SHADOW_GAP;
      }
    },

    /**
     * Handles the end of the horizontal tile grid transition.
     * @param {Event} e The tile grid webkitTransitionEnd event.
     */
    onTileGridTransitionEnd_: function(e) {
      if (!this.selected)
        return;

      // We should remove the classes that control transitions when the
      // transition ends so when the text is resized (Ctrl + '+'), no other
      // transition should happen except those defined in the specification.
      // For example, the tile has a transition for its 'width' property which
      // is used when the tile is being hidden. But when you resize the text,
      // and therefore the tile changes its 'width', this change should not be
      // animated.

      // When the tile grid width transition ends, we need to remove the class
      // 'animate-grid-width' which handles the tile grid width transition, and
      // individual tile transitions. TODO(pedrosimonetti): Investigate if we
      // can improve the performance here by using a more efficient selector.
      var tileGrid = this.tileGrid_;
      if (e.target == tileGrid &&
          tileGrid.classList.contains('animate-grid-width')) {
        tileGrid.classList.remove('animate-grid-width');

        if (this.onTileGridTransitionEndHandler_)
          this.onTileGridTransitionEndHandler_();
      }
    },
  };

  /**
   * Creates a new tile given a particular data. If there's no data, then
   * a tile filler will be created.
   * @param {TilePage} tilePage A TilePage.
   * @param {Object=} opt_data The data that will be used to create the tile.
   * @return {Tile} The new tile.
   */
  function createTile(tilePage, opt_data) {
    var tile;
    if (opt_data) {
      // If there's data, the new tile will be a real one (not a filler).
      tile = new tilePage.TileClass(tilePage.config);
      tile.setData(opt_data);
    } else {
      // Otherwise, it will be a fake filler tile.
      tile = cr.doc.createElement('span');
      tile.className = 'tile';
    }
    return tile;
  }

  /**
   * Fades a tile.
   * @param {Tile} tile A Tile.
   * @param {boolean} isFadeIn Whether to fade-in the tile. If |isFadeIn| is
   * false, then the tile is going to fade-out.
   */
  function fadeTile(tile, isFadeIn) {
    var className = 'animate-hide-tile';
    tile.classList.add(className);
    if (isFadeIn) {
      // Forces a reflow to ensure that the fade-out animation will work.
      tile.scrollTop;
      tile.classList.remove(className);
    }
  }

  return {
    Tile: Tile,
    TilePage: TilePage,
  };
});
