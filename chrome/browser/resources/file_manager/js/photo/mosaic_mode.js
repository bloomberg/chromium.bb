// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {Element} container Content container.
 * @param {cr.ui.ArrayDataModel} dataModel Data model.
 * @param {cr.ui.ListSelectionModel} selectionModel Selection model.
 * @param {MetadataCache} metadataCache Metadata cache.
 * @param {function} toggleMode Function to switch to the Slide mode.
 * @constructor
 */
function MosaicMode(container, dataModel, selectionModel,
                    metadataCache, toggleMode) {
  this.mosaic_ = new Mosaic(container.ownerDocument,
      dataModel, selectionModel, metadataCache);
  container.appendChild(this.mosaic_);

  this.toggleMode_ = toggleMode;
  this.mosaic_.addEventListener('dblclick', this.toggleMode_);
}

/**
 * @return {Mosaic} The mosaic control.
 */
MosaicMode.prototype.getMosaic = function() { return this.mosaic_ };

/**
 * @return {string} Mode name.
 */
MosaicMode.prototype.getName = function() { return 'mosaic' };

/**
 * Execute an action (this mode has no busy state).
 * @param {function} action Action to execute.
 */
MosaicMode.prototype.executeWhenReady = function(action) { action() };

/**
 * @return {boolean} Always true (no toolbar fading in this mode).
 */
MosaicMode.prototype.hasActiveTool = function() { return true };

/**
 * Keydown handler.
 *
 * @param {Event} event Event.
 * @return {boolean} True if processed.
 */
MosaicMode.prototype.onKeyDown = function(event) {
  switch (util.getKeyModifiers(event) + event.keyIdentifier) {
    case 'Enter':
      this.toggleMode_();
      return true;
  }
  return this.mosaic_.onKeyDown(event);
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Mosaic control.
 *
 * @param {Document} document Document.
 * @param {cr.ui.ArrayDataModel} dataModel Data model.
 * @param {cr.ui.ListSelectionModel} selectionModel Selection model.
 * @param {MetadataCache} metadataCache Metadata cache.
 * @return {Element} Mosaic element.
 * @constructor
 */
function Mosaic(document, dataModel, selectionModel, metadataCache) {
  var self = document.createElement('div');
  Mosaic.decorate(self, dataModel, selectionModel, metadataCache);
  return self;
}

/**
 * Inherit from HTMLDivElement.
 */
Mosaic.prototype.__proto__ = HTMLDivElement.prototype;

/**
 * Default layout delay in ms.
 */
Mosaic.LAYOUT_DELAY = 200;

/**
 * Decorate a Mosaic instance.
 *
 * @param {Mosaic} self Self pointer.
 * @param {cr.ui.ArrayDataModel} dataModel Data model.
 * @param {cr.ui.ListSelectionModel} selectionModel Selection model.
 * @param {MetadataCache} metadataCache Metadata cache.
 */
Mosaic.decorate = function(self, dataModel, selectionModel, metadataCache) {
  self.__proto__ = Mosaic.prototype;
  self.className = 'mosaic';

  self.dataModel_ = dataModel;
  self.selectionModel_ = selectionModel;
  self.metadataCache_ = metadataCache;

  // Initialization is completed lazily on the first call to |init|.
};

/**
 * Initialize the mosaic element.
 */
Mosaic.prototype.init = function() {
  if (this.tiles_)
    return; // Already initialized, nothing to do.

  this.layoutModel_ = new Mosaic.Layout(this);
  this.selectionController_ =
      new Mosaic.SelectionController(this.selectionModel_, this.layoutModel_);

  this.tiles_ = [];
  for (var i = 0; i != this.dataModel_.length; i++)
    this.tiles_.push(new Mosaic.Tile(this, this.dataModel_.item(i)));

  this.selectionModel_.selectedIndexes.forEach(function(index) {
    this.tiles_[index].select(true);
  }.bind(this));

  this.loadTiles_(this.tiles_);

  // The listeners might be called while some tiles are still loading.
  this.initListeners_();
};

/**
 * Start listening to events.
 *
 * We keep listening to events even when the mosaic is hidden in order to
 * keep the layout up to date.
 *
 * @private
 */
Mosaic.prototype.initListeners_ = function() {
  this.ownerDocument.defaultView.addEventListener(
      'resize', this.onResize_.bind(this));

  var mouseEventBound = this.onMouseEvent_.bind(this);
  this.addEventListener('mousemove', mouseEventBound);
  this.addEventListener('mousedown', mouseEventBound);
  this.addEventListener('mouseup', mouseEventBound);

  this.selectionModel_.addEventListener('change', this.onSelection_.bind(this));
  this.selectionModel_.addEventListener('leadIndexChange',
      this.onLeadChange_.bind(this));

  this.dataModel_.addEventListener('splice', this.onSplice_.bind(this));
  this.dataModel_.addEventListener('content', this.onContentChange_.bind(this));
};

/**
 * @return {Mosaic.Tile} Selected tile or undefined if no selection.
 */
Mosaic.prototype.getSelectedTile = function() {
  return this.tiles_ && this.tiles_[this.selectionModel_.selectedIndex];
};

/**
 * @param {number} index Tile index.
 * @return {Rect} Tile's image rectangle.
 */
Mosaic.prototype.getTileRect = function(index) {
  var tile = this.tiles_[index];
  return tile && tile.getImageRect();
};

/**
 * @param {number} index Tile index.
 * Scroll the given tile into the viewport.
 */
Mosaic.prototype.scrollIntoView = function(index) {
  var tile = this.tiles_[index];
  if (tile) tile.scrollIntoView();
};

/**
 * Load multiple tiles.
 *
 * @param {Array.<Mosaic.Tile>} tiles Array of tiles.
 * @param {function} opt_callback Completion callback.
 * @private
 */
Mosaic.prototype.loadTiles_ = function(tiles, opt_callback) {
  // We do not want to use tile indices in asynchronous operations because they
  // do not survive data model splices. Copy tile references instead.
  tiles = tiles.slice();

  // Throttle the metadata access so that we do not overwhelm the file system.
  var MAX_CHUNK_SIZE = 10;

  var loadChunk = function() {
    if (!tiles.length) {
      if (opt_callback) opt_callback();
      return;
    }
    var chunkSize = Math.min(tiles.length, MAX_CHUNK_SIZE);
    var loaded = 0;
    for (var i = 0; i != chunkSize; i++) {
      this.loadTile_(tiles.shift(), function() {
        if (++loaded == chunkSize) {
          this.layout();
          loadChunk();
        }
      }.bind(this));
    }
  }.bind(this);

  loadChunk();
};

/**
 * Load a single tile.
 *
 * @param {Mosaic.Tile} tile Tile.
 * @param {function} opt_callback Completion callback.
 * @private
 */
Mosaic.prototype.loadTile_ = function(tile, opt_callback) {
  this.metadataCache_.get(tile.getItem().getUrl(), Gallery.METADATA_TYPE,
      function(metadata) { tile.load(metadata, opt_callback) });
};

/**
 * Layout the tiles in the order of their indices.
 *
 * Starts where it last stopped (at #0 the first time).
 * Stops when all tiles are processed or when the next tile is still loading.
 */
Mosaic.prototype.layout = function() {
  if (this.layoutTimer_) {
    clearTimeout(this.layoutTimer_);
    this.layoutTimer_ = null;
  }
  while (true) {
    var index = this.layoutModel_.getTileCount();
    if (index == this.tiles_.length)
      break; // All tiles done.
    var tile = this.tiles_[index];
    if (!tile.isLoaded())
      break;  // Next layout will try to restart from here.
    this.layoutModel_.add(tile, index + 1 == this.tiles_.length);
  }
};

/**
 * Schedule the layout.
 *
 * @param {number} opt_delay Delay in ms.
 */
Mosaic.prototype.scheduleLayout = function(opt_delay) {
  if (!this.layoutTimer_) {
    this.layoutTimer_ = setTimeout(function() {
      this.layoutTimer_ = null;
      this.layout();
    }.bind(this), opt_delay || 0);
  }
};

/**
 * Resize handler.
 *
 * @private
 */
Mosaic.prototype.onResize_ = function() {
  this.layoutModel_.backtrack(0);  // Reset to tile 0.
  this.scheduleLayout();
};

/**
 * Mouse event handler.
 *
 * @param {Event} event Event.
 * @private
 */
Mosaic.prototype.onMouseEvent_ = function(event) {
  // Navigating with mouse, enable hover state.
  this.classList.add('hover-visible');

  if (event.type == 'mousemove')
    return;

  var index = -1;
  for (var target = event.target;
       target && (target != this);
       target = target.parentNode) {
    if (target.classList.contains('mosaic-tile')) {
      index = this.dataModel_.indexOf(target.getItem());
      break;
    }
  }
  this.selectionController_.handlePointerDownUp(event, index);
};

/**
 * Selection change handler.
 *
 * @param {Event} event Event.
 * @private
 */
Mosaic.prototype.onSelection_ = function(event) {
  for (var i = 0; i != event.changes.length; i++) {
    var change = event.changes[i];
    var tile = this.tiles_[change.index];
    if (tile) tile.select(change.selected);
  }
};

/**
 * Lead item change handler.
 *
 * @param {Event} event Event.
 * @private
 */
Mosaic.prototype.onLeadChange_ = function(event) {
  var index = event.newValue;
  if (index >= 0) {
    var tile = this.tiles_[index];
    if (tile) tile.scrollIntoView();
  }
};

/**
 * Splice event handler.
 *
 * @param {Event} event Event.
 * @private
 */
Mosaic.prototype.onSplice_ = function(event) {
  var index = event.index;
  this.layoutModel_.backtrack(index);

  if (event.removed.length) {
    for (var t = 0; t != event.removed.length; t++)
      this.removeChild(this.tiles_[index + t]);

    this.tiles_.splice(index, event.removed.length);
    this.scheduleLayout(Mosaic.LAYOUT_DELAY);
  }

  if (event.added.length) {
    var newTiles = [];
    for (var t = 0; t != event.added.length; t++)
      newTiles.push(new Mosaic.Tile(this, this.dataModel_.item(index + t)));

    this.tiles_.splice.apply(this.tiles_, [index, 0].concat(newTiles));
    this.loadTiles_(newTiles);
  }

  if (this.tiles_.length != this.dataModel_.length)
    console.error('Mosaic is out of sync');
};

/**
 * Content change handler.
 *
 * @param {Event} event Event.
 * @private
 */
Mosaic.prototype.onContentChange_ = function(event) {
  if (!this.tiles_)
    return;

  if (!event.metadata)
    return; // Thumbnail unchanged, nothing to do.

  var index = this.dataModel_.indexOf(event.item);
  if (index != this.selectionModel_.selectedIndex)
    console.error('Content changed for unselected item');

  this.layoutModel_.backtrack(index);
  this.tiles_[index].load(
      event.metadata, this.scheduleLayout.bind(this, Mosaic.LAYOUT_DELAY));
};

/**
 * Keydown event handler.
 *
 * @param {Event} event Event.
 * @return {boolean} True if the event has been consumed.
 */
Mosaic.prototype.onKeyDown = function(event) {
  this.selectionController_.handleKeyDown(event);
  if (event.defaultPrevented)  // Navigating with keyboard, hide hover state.
    this.classList.remove('hover-visible');
  return event.defaultPrevented;
};

/**
 * @return {boolean} True if the mosaic zoom effect can be applied. It is
 * too slow if there are to many images.
 * TODO(kaznacheev): Consider unloading the images that are out of the viewport.
 */
Mosaic.prototype.canZoom = function() {
  return this.layoutModel_.getTileCount() < 100;
};

/**
 * Show the mosaic.
 */
Mosaic.prototype.show = function() {
  var duration = ImageView.ZOOM_ANIMATION_DURATION;
  if (this.canZoom()) {
    // Fade in in parallel with the zoom effect.
    this.setAttribute('visible', 'zooming');
  } else {
    // Mosaic is not animating but the large image is. Fade in the mosaic
    // shortly before the large image animation is done.
    duration -= 100;
  }
  setTimeout(function() {
    // Make the selection visible.
    // If the mosaic is not animated it will start fading in now.
    this.setAttribute('visible', 'normal');
  }.bind(this), duration);

};

/**
 * Hide the mosaic.
 */
Mosaic.prototype.hide = function() {
  this.removeAttribute('visible');
};

/**
 * Apply or reset the zoom transform.
 *
 * @param {Rect} tileRect Tile rectangle. Reset the transform if null.
 * @param {Rect} imageRect Large image rectangle. Reset the transform if null.
 * @param {boolean} opt_instant True of the transition should be instant.
 */
Mosaic.prototype.transform = function(tileRect, imageRect, opt_instant) {
  if (opt_instant)
    this.style.webkitTransitionDuration = '0';
  else
    this.style.webkitTransitionDuration =
        ImageView.ZOOM_ANIMATION_DURATION + 'ms';

  if (this.canZoom() && tileRect && imageRect) {
    var scaleX = imageRect.width / tileRect.width;
    var scaleY = imageRect.height / tileRect.height;
    var shiftX = (imageRect.left + imageRect.width / 2) -
        (tileRect.left + tileRect.width / 2);
    var shiftY = (imageRect.top + imageRect.height / 2) -
        (tileRect.top + tileRect.height / 2);
    this.style.webkitTransform =
        'translate(' + shiftX * scaleX + 'px, ' + shiftY * scaleY + 'px)' +
        'scaleX(' + scaleX + ') scaleY(' + scaleY + ')';
  } else {
    this.style.webkitTransform = '';
  }
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Creates a selection controller that is to be used with grid.
 * @param {cr.ui.ListSelectionModel} selectionModel The selection model to
 *     interact with.
 * @param {Mosaic.Layout} layoutModel The layout model to use.
 * @constructor
 * @extends {!cr.ui.ListSelectionController}
 */
Mosaic.SelectionController = function(selectionModel, layoutModel) {
  cr.ui.ListSelectionController.call(this, selectionModel);
  this.layoutModel_ = layoutModel;
};

/**
 * Extends cr.ui.ListSelectionController.
 */
Mosaic.SelectionController.prototype.__proto__ =
    cr.ui.ListSelectionController.prototype;

/** @inheritDoc */
Mosaic.SelectionController.prototype.getLastIndex = function() {
  return this.layoutModel_.getLaidOutTileCount() - 1;
};

/** @inheritDoc */
Mosaic.SelectionController.prototype.getIndexBefore = function(index) {
  return this.layoutModel_.getAdjacentIndex(index, -1);
};

/** @inheritDoc */
Mosaic.SelectionController.prototype.getIndexAfter = function(index) {
  return this.layoutModel_.getAdjacentIndex(index, 1);
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Mosaic layout.
 *
 * @param {Element} container Container element.
 * @constructor
 */
Mosaic.Layout = function(container) {
  this.container_ = container;
  this.columns_ = [];
  this.newColumn_ = null;
};

/**
 * Blank space at the top of the mosaic element. We do not do that is CSS
 * because we want the mosaic to occupy the entire parent div which makes
 * animated transitions easier.
 */
Mosaic.Layout.PADDING_TOP = 50;

/**
 * Blank space at the bottom of the mosaic element.
 */
Mosaic.Layout.PADDING_BOTTOM = 70;

/**
 * Horizontal and vertical spacing between images. Should be kept in sync
 * with the style of .mosaic-item in gallery.css (= 2 * ( 4 + 1))
 */
Mosaic.Layout.SPACING = 10;

/**
 * @return {number} Total number of tiles added to the layout.
 */
Mosaic.Layout.prototype.getTileCount = function() {
  return this.getLaidOutTileCount() +
      (this.newColumn_ ? this.newColumn_.getTileCount() : 0);
};

/**
 * @return {number} Total number of tiles in completed columns.
 */
Mosaic.Layout.prototype.getLaidOutTileCount = function() {
  var lastColumn = this.columns_[this.columns_.length - 1];
  return lastColumn ? lastColumn.getLastTileIndex() : 0;
};

/**
 * Add a tile to the layout.
 *
 * @param {Mosaic.Tile} tile The tile to be added.
 * @param {boolean} isLast True if this tile is the last.
 */
Mosaic.Layout.prototype.add = function(tile, isLast) {
  var layoutQueue = [tile];
  var tilesPerRow = Mosaic.Row.TILES_PER_ROW;

  var layoutHeight = this.container_.clientHeight -
      (Mosaic.Layout.PADDING_TOP + Mosaic.Layout.PADDING_BOTTOM);

  while (layoutQueue.length) {
    if (!this.newColumn_) {
      this.newColumn_ = new Mosaic.Column(
          this.columns_.length, this.getTileCount(), layoutHeight, tilesPerRow);
      tilesPerRow = Mosaic.Row.TILES_PER_ROW;  // Reset the default.
    }

    this.newColumn_.add(layoutQueue.shift());

    if (this.newColumn_.prepareLayout(isLast && !layoutQueue.length)) {
      if (this.newColumn_.isSuboptimal()) {
        // Forget the new column, put its tiles back into the layout queue.
        layoutQueue = this.newColumn_.getTiles().concat(layoutQueue);
        tilesPerRow = 1;  // Force 1 tile per row for the next column.
      } else {
        var lastColumn = this.columns_[this.columns_.length - 1];
        this.newColumn_.layout(lastColumn ? lastColumn.getRightEdge() : 0,
            Mosaic.Layout.PADDING_TOP);
        this.columns_.push(this.newColumn_);
      }
      this.newColumn_ = null;
    }
  }
};

/**
 * Backtrack the layout to the beginning of the column containing a tile with
 * a given index.
 *
 * @param {number} index Tile index.
 */
Mosaic.Layout.prototype.backtrack = function(index) {
  this.newColumn_ = null;

  var columnIndex = this.getColumnIndexByTile_(index);
  if (columnIndex < 0)
    return; // Index not in the layout, probably already backtracked.

  this.columns_ = this.columns_.slice(0, columnIndex);
};

/**
 * Get the index of the tile to the left or to the right from the given tile.
 *
 * @param {number} index Tile index.
 * @param {number} direction -1 for left, 1 for right.
 * @return {number} Adjacent tile index.
 */
Mosaic.Layout.prototype.getAdjacentIndex = function(index, direction) {
  var column = this.getColumnIndexByTile_(index);
  if (column < 0) {
    console.error('Cannot find column for tile #' + index);
    return -1;
  }

  var row = this.columns_[column].getRowByTileIndex(index);
  if (!row) {
    console.error('Cannot find row for tile #' + index);
    return -1;
  }

  var sameRowNeighbourIndex = index + direction;
  if (row.hasTile(sameRowNeighbourIndex))
    return sameRowNeighbourIndex;

  var adjacentColumn = column + direction;
  if (adjacentColumn < 0 || adjacentColumn == this.columns_.length)
    return -1;

  return this.columns_[adjacentColumn].
      getEdgeTileIndex_(row.getCenterY(), -direction);
};

/**
 * @param {number} index Tile index.
 * @return {number} Index of the column containing the given tile.
 * @private
 */
Mosaic.Layout.prototype.getColumnIndexByTile_ = function(index) {
  for (var c = 0; c != this.columns_.length; c++) {
    if (this.columns_[c].hasTile(index))
      return c;
  }
  return -1;
};

/**
 * Scale the given array of size values to satisfy 3 conditions:
 * 1. The new sizes must be integer.
 * 2. The new sizes must sum up to the given |total| value.
 * 3. The relative proportions of the sizes should be as close to the original
 *    as possible.
 *
 * @param {Array.<number>} sizes Array of sizes.
 * @param {number} newTotal New total size.
 */
Mosaic.Layout.rescaleSizesToNewTotal = function(sizes, newTotal) {
  var total = 0;

  var partialTotals = [0];
  for (var i = 0; i != sizes.length; i++) {
    total += sizes[i];
    partialTotals.push(total);
  }

  var scale = newTotal / total;

  for (i = 0; i != sizes.length; i++) {
    sizes[i] = Math.round(partialTotals[i + 1] * scale) -
        Math.round(partialTotals[i] * scale);
  }
};

////////////////////////////////////////////////////////////////////////////////

/**
 * A column in a mosaic layout. Contains rows.
 *
 * @param {number} index Column index.
 * @param {number} firstTileIndex Index of the first tile in the column.
 * @param {number} maxHeight Maximum height.
 * @param {number} maxTilesPerRow Maximum number of tiles per row.
 * @constructor
 */
Mosaic.Column = function(index, firstTileIndex, maxHeight, maxTilesPerRow) {
  this.index_ = index;
  this.firstTileIndex_ = firstTileIndex;
  this.maxHeight_ = maxHeight;
  this.maxTilesPerRow_ = maxTilesPerRow;

  this.tiles_ = [];
  this.rows_ = [];
  this.newRow_ = null;
};

/**
 * @return {number} Number of tiles in the column.
 */
Mosaic.Column.prototype.getTileCount = function() { return this.tiles_.length };

/**
 * @return {number} Index of the last tile.
 */
Mosaic.Column.prototype.getLastTileIndex = function() {
  return this.firstTileIndex_ + this.getTileCount();
};

/**
 * @return {Array.<Mosaic.Tile>} Array of tiles in the column.
 */
Mosaic.Column.prototype.getTiles = function() { return this.tiles_ };

/**
 * @param {number} index Tile index.
 * @return {boolean} True if this column contains the tile with the given index.
 */
Mosaic.Column.prototype.hasTile = function(index) {
  return this.firstTileIndex_ <= index &&
      index < (this.firstTileIndex_ + this.getTileCount());
};

/**
 * @param {number} y Y coordinate.
 * @param {number} direction -1 for left, 1 for right.
 * @return {number} Index of the tile lying on the edge of the column at the
 *    given y coordinate.
 * @private
 */
Mosaic.Column.prototype.getEdgeTileIndex_ = function(y, direction) {
  for (var r = 0; r < this.rows_.length; r++) {
    if (this.rows_[r].coversY(y))
      return this.rows_[r].getEdgeTileIndex_(direction);
  }
  return -1;
};

/**
 * @param {number} index Tile index.
 * @return {Mosaic.Row} The row containing the tile with a given index.
 */
Mosaic.Column.prototype.getRowByTileIndex = function(index) {
  for (var r = 0; r != this.rows_.length; r++)
    if (this.rows_[r].hasTile(index))
      return this.rows_[r];

  return null;
};

/**
 * Add a tile to the column.
 *
 * @param {Mosaic.Tile} tile The tile to add.
 */
Mosaic.Column.prototype.add = function(tile) {
  if (!this.newRow_)
     this.newRow_ =
         new Mosaic.Row(this.firstTileIndex_ + this.getTileCount());


  this.tiles_.push(tile);
  this.newRow_.add(tile);

  // Force some images to occupy the entire row.
  // The empirical formula below creates fairly nice pattern.
  var modulo = (this.maxHeight_ < 700) ? 3 : 4;
  var forceFullRow =
      (this.rows_.length % modulo) == ([0, 2, 1, 3][this.index_ % modulo]);

  if (forceFullRow || (this.newRow_.getTileCount() == this.maxTilesPerRow_)) {
    this.rows_.push(this.newRow_);
    this.newRow_ = null;
  }
};

/**
 * Prepare the column layout.
 *
 * @param {boolean} opt_force True if the layout must be performed even for an
 *   incomplete column.
 * @return {boolean} True if the layout was performed.
 */
Mosaic.Column.prototype.prepareLayout = function(opt_force) {
  if (opt_force && this.newRow_) {
    this.rows_.push(this.newRow_);
    this.newRow_ = null;
  }

  if (this.rows_.length == 0)
    return false;

  this.width_ = Math.min.apply(
      null, this.rows_.map(function(row) { return row.getMaxWidth() }));

  var totalHeight = 0;

  this.rowHeights_ = [];
  for (var r = 0; r != this.rows_.length; r++) {
    var rowHeight = this.rows_[r].getHeightForWidth(this.width_);
    totalHeight += rowHeight;
    this.rowHeights_.push(rowHeight);
  }

  var overflow = totalHeight / this.maxHeight_;
  if (!opt_force && (overflow < 1))
    return false;

  if (overflow > 1) {
    // Scale down the column width and every row height.
    this.width_ = Math.round(this.width_ / overflow);
    Mosaic.Layout.rescaleSizesToNewTotal(this.rowHeights_, this.maxHeight_);
  }

  return true;
};

/**
 * @return {number} Column right edge coordinate after the layout.
 */
Mosaic.Column.prototype.getRightEdge = function() {
  return this.left_ + this.width_;
};

/**
 * Perform the column layout.
 *
 * @param {number} left Left coordinate.
 * @param {number} top Top coordinate.
 */
Mosaic.Column.prototype.layout = function(left, top) {
  this.left_ = left;
  for (var r = 0; r != this.rows_.length; r++) {
    this.rows_[r].layout(left, top, this.width_, this.rowHeights_[r]);
    top += this.rowHeights_[r];
  }
};

/**
 * Check if the column layout is too ugly to be displayed.
 *
 * @return {boolean} True if the layout is suboptimal.
 */
Mosaic.Column.prototype.isSuboptimal = function() {
  var sizes =
      this.tiles_.map(function(tile) { return tile.getMaxContentHeight() });
  var tileCounts =
      this.rows_.map(function(row) { return row.getTileCount() });

  // Ugly layout #1: all images are small and some are one the same row.
  var allSmall = Math.max.apply(null, sizes) <= Mosaic.Tile.SMALL_IMAGE_SIZE;
  var someCombined = Math.max.apply(null, tileCounts) != 1;
  if (allSmall && someCombined)
    return true;

  // Ugly layout #2: all images are large and none occupies an entire row.
  var allLarge = Math.min.apply(null, sizes) > Mosaic.Tile.SMALL_IMAGE_SIZE;
  var allCombined = Math.min.apply(null, tileCounts) != 1;

  if (allLarge && allCombined)
    return true;

  return false;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * A row in a mosaic layout. Contains tiles.
 *
 * @param {number} firstTileIndex Index of the first tile in the row.
 * @constructor
 */
Mosaic.Row = function(firstTileIndex) {
  this.firstTileIndex_ = firstTileIndex;
  this.tiles_ = [];
};

/**
 * Default number of tiles per row.
 * @type {Number}
 */
Mosaic.Row.TILES_PER_ROW = 2;

/**
 * @param {Mosaic.Tile} tile The tile to add.
 */
Mosaic.Row.prototype.add = function(tile) {
  if (this.isFull())
    console.error('Inconsistent state');

  this.tiles_.push(tile);
};

/**
 *
 * @return {number} Number of tiles in the row.
 */
Mosaic.Row.prototype.getTileCount = function() { return this.tiles_.length };

/**
 * @return {boolean} True if the row is full.
 */
Mosaic.Row.prototype.isFull = function() {
  return this.tiles_.length >= 2;
};

/**
 * @param {number} index Tile index.
 * @return {boolean} True if this row contains the tile with the given index.
 */
Mosaic.Row.prototype.hasTile = function(index) {
  return this.firstTileIndex_ <= index &&
      index < (this.firstTileIndex_ + this.tiles_.length);
};

/**
 * @param {number} y Y coordinate.
 * @return {boolean} True if this row covers the given Y coordinate.
 */
Mosaic.Row.prototype.coversY = function(y) {
  return this.top_ <= y && y < (this.top_ + this.height_);
};

/**
 * @return {number} Y coordinate of the tile center.
 */
Mosaic.Row.prototype.getCenterY = function() {
  return this.top_ + Math.round(this.height_ / 2);
};

/**
 * Get the first or the last tile.
 *
 * @param {number} direction -1 for the first tile, 1 for the last tile.
 * @return {number} Tile index.
 * @private
 */
Mosaic.Row.prototype.getEdgeTileIndex_ = function(direction) {
  if (direction < 0)
    return this.firstTileIndex_;
  else
    return this.firstTileIndex_ + this.getTileCount() - 1;
};

/**
 * @return {number} Aspect ration of the combined content box of this row.
 * @private
 */
Mosaic.Row.prototype.getTotalContentAspectRatio_ = function() {
  var sum = 0;
  for (var t = 0; t != this.tiles_.length; t++)
    sum += this.tiles_[t].getAspectRatio();
  return sum;
};

/**
 * @return {number} Total horizontal spacing in this row. This includes
 *   the spacing between the tiles and both left and right margins.
 *
 * @private
 */
Mosaic.Row.prototype.getTotalHorizontalSpacing_ = function() {
  return Mosaic.Layout.SPACING * this.getTileCount();
};

/**
 * @return {number} Maximum width that this row may have without overscaling
 * any of the tiles.
 */
Mosaic.Row.prototype.getMaxWidth = function() {
  var contentHeight = Math.min.apply(null,
      this.tiles_.map(function(tile) { return tile.getMaxContentHeight() }));

  var contentWidth =
      Math.round(contentHeight * this.getTotalContentAspectRatio_());
  return contentWidth + this.getTotalHorizontalSpacing_();
};

/**
 * Compute the height that best fits the supplied row width given
 * aspect ratios of the tiles in this row.
 *
 * @param {number} width Row width.
 * @return {number} Height.
 */
Mosaic.Row.prototype.getHeightForWidth = function(width) {
  var contentWidth = width - this.getTotalHorizontalSpacing_();
  var contentHeight =
      Math.round(contentWidth / this.getTotalContentAspectRatio_());
  return contentHeight + Mosaic.Layout.SPACING;
};

/**
 * Position the row in the mosaic.
 *
 * @param {number} left Left position.
 * @param {number} top Top position.
 * @param {number} width Width.
 * @param {number} height Height.
 */
Mosaic.Row.prototype.layout = function(left, top, width, height) {
  this.top_ = top;
  this.height_ = height;

  var contentWidth = width - this.getTotalHorizontalSpacing_();
  var contentHeight = height - Mosaic.Layout.SPACING;

  var tileContentWidth = this.tiles_.map(
      function(tile) { return tile.getAspectRatio() });

  Mosaic.Layout.rescaleSizesToNewTotal(tileContentWidth, contentWidth);

  var tileLeft = left;
  for (var t = 0; t != this.tiles_.length; t++) {
    var tileWidth = tileContentWidth[t] + Mosaic.Layout.SPACING;
    this.tiles_[t].layout(tileLeft, top, tileWidth, height);
    tileLeft += tileWidth;
  }
};

////////////////////////////////////////////////////////////////////////////////

/**
 * A single tile of the image mosaic.
 *
 * @param {Element} container Container element.
 * @param {Gallery.Item} item Gallery item associated with this tile.
 * @return {Element} The new tile element.
 * @constructor
 */
Mosaic.Tile = function(container, item) {
  var self = container.ownerDocument.createElement('div');
  Mosaic.Tile.decorate(self, container, item);
  return self;
};

/**
 * @param {Element} self Self pointer.
 * @param {Element} container Container element.
 * @param {Gallery.Item} item Gallery item associated with this tile.
 */
Mosaic.Tile.decorate = function(self, container, item) {
  self.__proto__ = Mosaic.Tile.prototype;
  self.className = 'mosaic-tile';

  self.container_ = container;
  self.item_ = item;
  self.left_ = null; // Mark as not laid out.
};

/**
* Inherit from HTMLDivElement.
*/
Mosaic.Tile.prototype.__proto__ = HTMLDivElement.prototype;

/**
 * Minimum tile content size.
 */
Mosaic.Tile.MIN_CONTENT_SIZE = 64;

/**
 * Maximum tile content size.
 */
Mosaic.Tile.MAX_CONTENT_SIZE = 512;

/**
 * Default size for a tile with no thumbnail image.
 */
Mosaic.Tile.GENERIC_ICON_SIZE = 128;

/**
 * Max size of an image considered to be 'small'.
 * Small images are laid out slightly differently.
 */
Mosaic.Tile.SMALL_IMAGE_SIZE = 160;

/**
 * @return {Gallery.Item} The Gallery item.
 */
Mosaic.Tile.prototype.getItem = function() { return this.item_ };

/**
 * @return {number} Maximum content height that this tile can have.
 */
Mosaic.Tile.prototype.getMaxContentHeight = function() {
  return this.maxContentHeight_;
};

/**
 * @return {number} The aspect ratio of the tile image.
 */
Mosaic.Tile.prototype.getAspectRatio = function() { return this.aspectRatio_ };

/**
 * @return {boolean} True if the tile is loaded.
 */
Mosaic.Tile.prototype.isLoaded = function() { return !!this.maxContentHeight_ };

/**
 * Load the thumbnail image into the tile.
 *
 * @param {object} metadata Metadata object.
 * @param {function} opt_callback Completion callback.
 */
Mosaic.Tile.prototype.load = function(metadata, opt_callback) {
  this.maxContentHeight_ = 0;  // Prevent layout of this while loading.
  this.left_ = null;  // Mark as not laid out.

  this.thumbnailLoader_ =
      new ThumbnailLoader(this.getItem().getUrl(), metadata);

  this.thumbnailLoader_.loadDetachedImage(function() {
    if (this.thumbnailLoader_.hasValidImage()) {
      var width = this.thumbnailLoader_.getWidth();
      var height = this.thumbnailLoader_.getHeight();
      if (width > height) {
        if (width > Mosaic.Tile.MAX_CONTENT_SIZE) {
          height = Math.round(height * Mosaic.Tile.MAX_CONTENT_SIZE / width);
          width = Mosaic.Tile.MAX_CONTENT_SIZE;
        }
      } else {
        if (height > Mosaic.Tile.MAX_CONTENT_SIZE) {
          width = Math.round(width * Mosaic.Tile.MAX_CONTENT_SIZE / height);
          height = Mosaic.Tile.MAX_CONTENT_SIZE;
        }
      }
      this.maxContentHeight_ = Math.max(Mosaic.Tile.MIN_CONTENT_SIZE, height);
      this.aspectRatio_ = width / height;
    } else {
      this.maxContentHeight_ = Mosaic.Tile.GENERIC_ICON_SIZE;
      this.aspectRatio_ = 1;
    }

    if (opt_callback) opt_callback();
  }.bind(this));
};

/**
 * Select/unselect the tile.
 *
 * @param {boolean} on True if selected.
 */
Mosaic.Tile.prototype.select = function(on) {
  if (on)
    this.setAttribute('selected', true);
  else
    this.removeAttribute('selected');
};

/**
 * Position the tile in the mosaic.
 *
 * @param {number} left Left position.
 * @param {number} top Top position.
 * @param {number} width Width.
 * @param {number} height Height.
 */
Mosaic.Tile.prototype.layout = function(left, top, width, height) {
  this.left_ = left;
  this.top_ = top;
  this.width_ = width;
  this.height_ = height;

  this.style.left = left + 'px';
  this.style.top = top + 'px';
  this.style.width = width + 'px';
  this.style.height = height + 'px';

  if (!this.wrapper_) {  // First time, create DOM.
    this.container_.appendChild(this);
    var border = util.createChild(this, 'img-border');
    this.wrapper_ = util.createChild(border, 'img-wrapper');
  }
  if (this.hasAttribute('selected'))
    this.scrollIntoView();

  this.thumbnailLoader_.attachImage(this.wrapper_, true /* fill */);
};

/**
 * If the tile is not fully visible scroll the parent to make it fully visible.
 */
Mosaic.Tile.prototype.scrollIntoView = function() {
  if (this.left_ == null)  // Not laid out.
    return;

  if (this.left_ < this.container_.scrollLeft) {
    this.container_.scrollLeft = this.left_;
  } else {
    var tileRight = this.left_ + this.width_;
    var scrollRight = this.container_.scrollLeft + this.container_.clientWidth;
    if (tileRight > scrollRight)
      this.container_.scrollLeft = tileRight - this.container_.clientWidth;
  }
};

/**
 * @return {Rect} Rectangle occupied by the tile's image,
 *   relative to the viewport.
 */
Mosaic.Tile.prototype.getImageRect = function() {
  if (this.left_ == null)  // Not laid out.
    return null;

  var margin = Mosaic.Layout.SPACING / 2;
  return new Rect(this.left_ - this.container_.scrollLeft, this.top_,
      this.width_, this.height_).inflate(-margin, -margin);
};
