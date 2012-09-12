// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp', function() {
  'use strict';

  //----------------------------------------------------------------------------
  // Constants
  //----------------------------------------------------------------------------

  /**
   * The height required to show 2 rows of Tiles in the Bottom Panel.
   * @type {number}
   * @const
   */
  var HEIGHT_FOR_TWO_ROWS = 275;

  /**
   * The height required to show the Bottom Panel.
   * @type {number}
   * @const
   */
  var HEIGHT_FOR_BOTTOM_PANEL = 170;

  /**
   * The Bottom Panel width required to show 5 cols of Tiles, which is used
   * in the width computation.
   * @type {number}
   * @const
   */
  var MAX_BOTTOM_PANEL_WIDTH = 948;

  /**
   * The normal Bottom Panel width. If the window width is greater than or
   * equal to this value, then the width of the Bottom Panel's content will be
   * the available width minus side margin. If the available width is smaller
   * than this value, then the width of the Bottom Panel's content will be an
   * interpolation between the normal width, and the minimum width defined by
   * the constant MIN_BOTTOM_PANEL_CONTENT_WIDTH.
   * @type {number}
   * @const
   */
  var NORMAL_BOTTOM_PANEL_WIDTH = 500;

  /**
   * The minimum Bottom Panel width. If the available width is smaller than
   * this value, then the width of the Bottom Panel's content will be fixed to
   * MIN_BOTTOM_PANEL_CONTENT_WIDTH.
   * @type {number}
   * @const
   */
  var MIN_BOTTOM_PANEL_WIDTH = 300;

  /**
   * The minimum width of the Bottom Panel's content.
   * @type {number}
   * @const
   */
  var MIN_BOTTOM_PANEL_CONTENT_WIDTH = 200;

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
    /**
     * Initializes a Tile.
     * @param {Object} config TilePage configuration object.
     */
    initialize: function(config) {
      this.className = 'tile';
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
      this.assign_(tile);
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
    assign_: function(tile) {
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
    }
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
    el.initialize();

    return el;
  }

  TilePage.prototype = {
    __proto__: HTMLDivElement.prototype,

    // The config object should be defined by each TilePage subclass.
    config_: {
      // The width of a cell.
      cellWidth: 0,
      // The start margin of a cell (left or right according to text direction).
      cellMarginStart: 0,
      // The border panel horizontal margin.
      bottomPanelHorizontalMargin: 0,
      // The height of the tile row.
      rowHeight: 0,
      // The maximum number of Tiles to be displayed.
      maxTileCount: 0
    },

    /**
     * Initializes a TilePage.
     */
    initialize: function() {
      this.className = 'tile-page';

      // The content defines the actual space a page has to display tiles.
      this.content_ = this.ownerDocument.createElement('div');
      this.content_.className = 'tile-page-content';
      this.appendChild(this.content_);

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

      // Event handlers.
      this.tileGrid_.addEventListener('webkitTransitionEnd',
          this.onTileGridTransitionEnd_.bind(this));

      $('page-list').addEventListener('webkitTransitionEnd',
          this.onPageListTransitionEnd_.bind(this));

      this.eventTracker = new EventTracker();
      this.eventTracker.add(window, 'resize', this.onResize_.bind(this));
      this.eventTracker.add(window, 'keyup', this.onKeyUp_.bind(this));

      this.addEventListener('cardselected', this.handleCardSelection_);
      this.addEventListener('carddeselected', this.handleCardDeselection_);
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
     * The notification content of this tile (if any, otherwise null).
     * @type {!HTMLElement}
     */
    get notification() {
      return this.content_.firstChild.id == 'notification-container' ?
          this.content_.firstChild : null;
    },
    /**
     * The notification content of this tile (if any, otherwise null).
     * @type {!HTMLElement}
     */
    set notification(node) {
      assert(node instanceof HTMLElement, '|node| isn\'t an HTMLElement!');
      // NOTE: Implicitly removes from DOM if |node| is inside it.
      this.content_.insertBefore(node, this.content_.firstChild);
      this.positionNotification_();
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
      this.tearDown_();
      this.parentNode.removeChild(this);
    },

    /**
     * Cleans up resources that are no longer needed after this TilePage
     * instance is removed from the DOM.
     * @private
     */
    tearDown_: function() {
      this.eventTracker.removeAll();
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
      if (opt_animate)
        this.classList.add('animating-tile-page');
      var index = tile.index;
      tile.parentNode.removeChild(tile);
      this.layout_();
      this.cleanupDrag();

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
      // TODO(pedrosimonetti): Dispatch individual tearDown functions.
      this.tileGrid_.innerHTML = '';
    },

    /**
     * Called when the page is selected (in the card selector).
     * @param {Event} e A custom cardselected event.
     * @private
     */
    handleCardSelection_: function(e) {
      this.layout_();
    },

    /**
     * Called when the page loses selection (in the card selector).
     * @param {Event} e A custom carddeselected event.
     * @private
     */
    handleCardDeselection_: function(e) {
    },

    /**
     * Position the notification if there's one showing.
     * TODO(pedrosimonetti): Fix the position of the notification.
     */
    positionNotification_: function() {
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
    numOfVisibleRows_: 0,
    // The number of the last column being animated. We initialize this value
    // with zero so we can detect when the first time the page is rendered.
    animatingColCount_: 0,
    // The index of the topmost row visible.
    pageOffset_: 0,

    /**
     * Appends a tile to the end of the tile grid.
     * @param {Tile} tile The tile to be added.
     * @param {number} index The location in the tile grid to insert it at.
     * @protected
     */
    appendTile: function(tile) {
      var index = this.tiles_.length;
      this.tiles_.push(tile);
      this.fireAddedEvent(tile, index);
    },

    /**
     * Adds the given element to the tile grid.
     * TODO(pedrosimonetti): If this is not being used, delete.
     * @param {Tile} tile The tile to be added.
     * @param {number} index The location in the tile grid to insert it at.
     * @protected
     */
    addTileAt: function(tile, index) {
      this.tiles_.splice(index, 0, tile);
      this.fireAddedEvent(tile, index);
    },

    // internal helpers
    // -------------------------------------------------------------------------

    /**
     * Gets the required width for a Tile.
     * @private
     */
    getTileRequiredWidth_: function() {
      var conf = this.config_;
      return conf.cellWidth + conf.cellMarginStart;
    },

    /**
     * Gets the the maximum number of columns that can fit in a given width.
     * @param {number} width The width in pixels.
     * @private
     */
    getColCountForWidth_: function(width) {
      var availableWidth = width + this.config_.cellMarginStart;
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
      var width = colCount * requiredWidth - this.config_.cellMarginStart;
      return width;
    },

    /**
     * Gets the bottom panel width.
     * @private
     */
    getBottomPanelWidth_: function() {
      var windowWidth = cr.doc.documentElement.clientWidth;
      var margin = 2 * this.config_.bottomPanelHorizontalMargin;
      var width;
      if (windowWidth >= MAX_BOTTOM_PANEL_WIDTH) {
        width = MAX_BOTTOM_PANEL_WIDTH - margin;
      } else if (windowWidth >= NORMAL_BOTTOM_PANEL_WIDTH) {
        width = windowWidth - margin;
      } else if (windowWidth >= MIN_BOTTOM_PANEL_WIDTH) {
        // Interpolation between the previous and next states.
        var minMargin = MIN_BOTTOM_PANEL_WIDTH - MIN_BOTTOM_PANEL_CONTENT_WIDTH;
        var factor = (windowWidth - MIN_BOTTOM_PANEL_WIDTH) /
            (NORMAL_BOTTOM_PANEL_WIDTH - MIN_BOTTOM_PANEL_WIDTH);
        var interpolatedMargin = minMargin + factor * (margin - minMargin);
        width = windowWidth - interpolatedMargin;
      } else {
        width = MIN_BOTTOM_PANEL_CONTENT_WIDTH;
      }

      return width;
    },

    /**
     * Gets the number of available columns.
     * @private
     */
    getAvailableColCount_: function() {
      return this.getColCountForWidth_(this.getBottomPanelWidth_());
    },

    /**
     * @return {boolean} Whether the page has been rendered.
     */
    hasBeenRendered: function() {
      return this.numOfVisibleRows_ != 0 || this.animatingColCount_ != 0;
    },

    // rendering
    // -------------------------------------------------------------------------

    /**
     * Renders the grid.
     * @param {number} colCount The number of columns.
     * @private
     */
    renderGrid_: function(colCount) {
      colCount = colCount || this.colCount_;

      var tileGridContent = this.tileGridContent_;
      var tiles = this.tiles_;
      var tileCount = tiles.length;

      var numOfVisibleRows = this.numOfVisibleRows_;
      var rowCount = Math.ceil(tileCount / colCount);
      rowCount = Math.max(rowCount, numOfVisibleRows);
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

        // Adjust row visibility.
        var rowVisible = row >= pageOffset &&
            row <= (pageOffset + numOfVisibleRows - 1);
        this.showTileRow_(tileRow, rowVisible);

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
            tileCell = new TileCell(span, this.config_);
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
    },

    /**
     * Adjusts the layout of the tile page according to the current window size.
     * @private
     */
    layout_: function() {
      // Only adjusts the layout if the page is currently selected, and we have
      // tiles to render.
      if (!this.selected || this.tiles_.length == 0)
        return;

      var bottomPanelWidth = this.getBottomPanelWidth_();
      var colCount = this.getColCountForWidth_(bottomPanelWidth);
      var lastColCount = this.colCount_;
      var animatingColCount = this.animatingColCount_;

      var windowHeight = cr.doc.documentElement.clientHeight;

      // We should not animate the very first layout, that is, when the page
      // has not been rendered.
      var shouldAnimate = this.hasBeenRendered();

      this.showBottomPanel_(windowHeight >= HEIGHT_FOR_BOTTOM_PANEL);

      // TODO(pedrosimonetti): Add constants?
      // If the number of visible rows has changed, then we need to resize the
      // grid and animate the affected rows. We also need to keep track of
      // whether the number of visible rows has changed because we might have
      // to render the grid when the number of columns hasn't changed.
      var numberOfRowsHasChanged = false;
      var numOfVisibleRows = windowHeight > HEIGHT_FOR_TWO_ROWS ? 2 : 1;
      if (numOfVisibleRows != this.numOfVisibleRows_) {
        this.numOfVisibleRows_ = numOfVisibleRows;
        numberOfRowsHasChanged = true;

        var pageList = $('page-list');
        if (shouldAnimate)
          pageList.classList.add('animate-page-height');

        // By forcing the pagination, all affected rows will be animated.
        this.paginate_(null, true);
        pageList.style.height =
            (this.config_.rowHeight * numOfVisibleRows) + 'px';
      }

      // If the number of columns being animated has changed, then we need to
      // resize the grid, animate the tiles, and render the grid when its actual
      // size changes.
      if (colCount != animatingColCount) {
        if (shouldAnimate)
          this.tileGrid_.classList.add('animate-grid-width');

        var newWidth = this.getWidthForColCount_(colCount);

        // If the grid is expanding horizontally we need to render the grid
        // first so the revealing tiles are visible as soon as the animation
        // starts.
        if (colCount > animatingColCount) {
          // We only have to render the grid if the actual number of columns
          // has changed.
          if (colCount != lastColCount)
            this.renderGrid_(colCount);

          // Animates the affected columns.
          this.showTileCols_(animatingColCount, false);

          var self = this;
          // We need to save the animatingColCount value in a closure otherwise
          // the animation of tiles won't work when expanding horizontally.
          // The problem happens because the layout_ method is called several
          // times when resizing the window, and the animatingColCount is saved
          // and restored each time a new column has to be animated. So, if we
          // don't save the value, by the time the showTileCols_ method is
          // called the animatingColCount is holding a new value, breaking
          // the animation.
          setTimeout((function(animatingColCount) {
            return function() {
              self.showTileCols_(animatingColCount, true);
            }
          })(animatingColCount), 0);

        } else {
          // If the grid is shrinking horizontally, then we need to render the
          // grid only after the animation ends so the tiles that are being
          // hidden remains visible until the end of the animation. See
          // onTileGridTransitionEndHandler_ below.
          this.showTileCols_(colCount, false);
        }

        this.tileGrid_.style.width = newWidth + 'px';
        $('page-list-menu').style.width = newWidth + 'px';

        var self = this;
        this.onTileGridTransitionEndHandler_ = function() {
          if (colCount < lastColCount)
            self.renderGrid_(colCount);
          else
            self.showTileCols_(0, true);
        };

        this.paginate_();

      } else if (numberOfRowsHasChanged) {
        // If the number of columns being animated hasn't changed but the number
        // of rows has changed then we have to render the grid too.
        this.renderGrid_(colCount);
      }

      this.content_.style.width = bottomPanelWidth + 'px';

      this.animatingColCount_ = colCount;
    },

    // animation helpers
    // -------------------------------------------------------------------------

    /**
     * Animates the display the Bottom Panel.
     * @param {boolean} show Whether or not to show the Bottom Panel.
     */
    showBottomPanel_: function(show) {
      $('card-slider-frame').classList[show ? 'remove' : 'add'](
          'hide-card-slider');
    },

    /**
     * Animates the display of a row. TODO(pedrosimonetti): Make it local?
     * @param {HTMLElement} row The row element.
     * @param {boolean} show Whether or not to show the row.
     */
    showTileRow_: function(row, show) {
      row.classList[show ? 'remove' : 'add']('hide-row');
    },

    /**
     * Animates the display of columns. TODO(pedrosimonetti): Make it local?
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

    // pagination
    // -------------------------------------------------------------------------

    /**
     * Resets the display of columns.
     * @param {number=} opt_pageOffset The index of the topmost row visible.
     *     Default value is the last pageOffset_.
     * @param {boolean=} opt_force Forces the pagination. Default is false.
     */
    paginate_: function(opt_pageOffset, opt_force) {
      var numOfVisibleRows = this.numOfVisibleRows_;
      var pageOffset = typeof opt_pageOffset == 'number' ?
          opt_pageOffset : this.pageOffset_;

      pageOffset = Math.min(this.rowCount_ - numOfVisibleRows, pageOffset);
      pageOffset = Math.max(0, pageOffset);

      if (pageOffset != this.pageOffset || opt_force) {
        var rows = this.tileGridContent_.getElementsByClassName('tile-row');
        for (var i = 0, length = rows.length; i < length; i++) {
          var row = rows[i];
          var isRowVisible = i >= pageOffset &&
              i <= (pageOffset + numOfVisibleRows - 1);
          this.showTileRow_(row, isRowVisible);
        }

        this.pageOffset_ = pageOffset;
        this.tileGridContent_.style.webkitTransform =
            'translate3d(0,' + (-pageOffset * this.config_.rowHeight) + 'px,0)';
      }
    },

    // event handlers
    // -------------------------------------------------------------------------

    /**
     * Handles the window resize event.
     * @param {Event} e The window resize event.
     */
    onResize_: function(e) {
      this.layout_();
    },

    /**
     * Handles the end of the horizontal tile grid transition.
     * @param {Event} e The tile grid webkitTransitionEnd event.
     */
    onTileGridTransitionEnd_: function(e) {
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
      if (event.target == tileGrid &&
          tileGrid.classList.contains('animate-grid-width')) {
        tileGrid.classList.remove('animate-grid-width');

        if (this.onTileGridTransitionEndHandler_)
          this.onTileGridTransitionEndHandler_();
      }
    },

    /**
     * Handles the end of the vertical page list transition.
     * @param {Event} e The tile grid webkitTransitionEnd event.
     */
    onPageListTransitionEnd_: function(e) {
      // For the same reason as explained in onTileGridTransitionEnd_, we need
      // to remove the class 'animate-page-height' when the vertical transition
      // ends.
      var pageList = $('page-list');
      if (event.target == pageList &&
          pageList.classList.contains('animate-page-height')) {
        pageList.classList.remove('animate-page-height');
      }
    },

    /**
     * Handles the window keyup event.
     * @param {Event} e The keyboard event.
     */
    onKeyUp_: function(e) {
      var pageOffset = this.pageOffset_;

      var keyCode = e.keyCode;
      if (keyCode == 40 /* down */)
        pageOffset++;
      else if (keyCode == 38 /* up */)
        pageOffset--;

      // Changes the pagination according to which arrow key was pressed.
      if (pageOffset != this.pageOffset_)
        this.paginate_(pageOffset);
    }
  };

  /**
   * Shows a deprecate error.
   */
  function deprecate() {
    console.error('This function is deprecated!');
  }

  return {
    // TODO(pedrosimonetti): Drag. Delete after porting the rest of the code.
    getCurrentlyDraggingTile2: deprecate,
    setCurrentDropEffect2: deprecate,
    Tile2: Tile,
    TilePage2: TilePage
  };
});
