// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {Element} container Content container.
 * @param {cr.ui.ArrayDataModel} dataModel Data model.
 * @param {cr.ui.ListSelectionModel} selectionModel Selection model.
 * @param {MetadataCache} metadataCache Metadata cache.
 * @param {function} toggleMode Function to switch to the Slide mode.
 * @param {function(string)} onThumbnailError Thumbnail load error handler.
 * @constructor
 */
function MosaicMode(container, dataModel, selectionModel,
                    metadataCache, toggleMode, onThumbnailError) {
  this.mosaic_ = new Mosaic(container.ownerDocument,
      dataModel, selectionModel, metadataCache, onThumbnailError);
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
 * @param {function(string)} onThumbnailError Thumbnail load error handler.
 * @return {Element} Mosaic element.
 * @constructor
 */
function Mosaic(document, dataModel, selectionModel, metadataCache,
                onThumbnailError) {
  var self = document.createElement('div');
  Mosaic.decorate(self,
      dataModel, selectionModel, metadataCache, onThumbnailError);
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
 * Smooth scroll animation duration when scrolling using keyboard or
 * clicking on a partly visible tile. In ms.
 */
Mosaic.ANIMATED_SCROLL_DURATION = 500;

/**
 * Decorate a Mosaic instance.
 *
 * @param {Mosaic} self Self pointer.
 * @param {cr.ui.ArrayDataModel} dataModel Data model.
 * @param {cr.ui.ListSelectionModel} selectionModel Selection model.
 * @param {MetadataCache} metadataCache Metadata cache.
 * @param {function(string)} onThumbnailError Thumbnail load error handler.
 */
Mosaic.decorate = function(self, dataModel, selectionModel, metadataCache,
                           onThumbnailError) {
  self.__proto__ = Mosaic.prototype;
  self.className = 'mosaic';

  self.dataModel_ = dataModel;
  self.selectionModel_ = selectionModel;
  self.metadataCache_ = metadataCache;
  self.onThumbnailError_ = onThumbnailError;

  // Initialization is completed lazily on the first call to |init|.
};

/**
 * Initialize the mosaic element.
 */
Mosaic.prototype.init = function() {
  if (this.tiles_)
    return; // Already initialized, nothing to do.

  this.layoutModel_ = new Mosaic.Layout();
  this.onResize_();

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
 * @return {boolean} Whether mosaic is initialized.
 */
Mosaic.prototype.isInitialized = function() {
  return !!this.tiles_;
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
 * Smoothly scrolls the container to the specified position using
 * f(x) = sqrt(x) speed function normalized to animation duration.
 * @param {number} targetPosition Horizontal scroll position in pixels.
 */
Mosaic.prototype.animatedScrollTo = function(targetPosition) {
  if (this.scrollAnimation_) {
    webkitCancelAnimationFrame(this.scrollAnimation_);
    this.scrollAnimation_ = null;
  }

  // Mouse move events are fired without touching the mouse because of scrolling
  // the container. Therefore, these events have to be suppressed.
  this.suppressHovering_ = true;

  // Calculates integral area from t1 to t2 of f(x) = sqrt(x) dx.
  var integral = function(t1, t2) {
    return 2.0 / 3.0 * Math.pow(t2, 3.0 / 2.0) -
           2.0 / 3.0 * Math.pow(t1, 3.0 / 2.0);
  };

  var delta = targetPosition - this.scrollLeft;
  var factor = delta / integral(0, Mosaic.ANIMATED_SCROLL_DURATION);
  var startTime = Date.now();
  var lastPosition = 0;
  var scrollOffset = this.scrollLeft;

  var animationFrame = function() {
    var position = Date.now() - startTime;
    var step = factor *
        integral(Math.max(0, Mosaic.ANIMATED_SCROLL_DURATION - position),
                 Math.max(0, Mosaic.ANIMATED_SCROLL_DURATION - lastPosition));
    scrollOffset += step;

    var oldScrollLeft = this.scrollLeft;
    var newScrollLeft = Math.round(scrollOffset);

    if (oldScrollLeft != newScrollLeft)
      this.scrollLeft = newScrollLeft;

    if (step == 0 || this.scrollLeft != newScrollLeft) {
      this.scrollAnimation_ = null;
      // Release the hovering lock after a safe delay to avoid hovering
      // a tile because of altering |this.scrollLeft|.
      setTimeout(function() {
        if (!this.scrollAnimation_)
          this.suppressHovering_ = false;
      }.bind(this), 100);
    } else {
      // Continue the animation.
      this.scrollAnimation_ = requestAnimationFrame(animationFrame);
    }

    lastPosition = position;
  }.bind(this);

  // Start the animation.
  this.scrollAnimation_ = requestAnimationFrame(animationFrame);
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
 * @param {function=} opt_callback Completion callback.
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
 * @param {function} callback Completion callback.
 * @private
 */
Mosaic.prototype.loadTile_ = function(tile, callback) {
  var url = tile.getItem().getUrl();
  var onImageLoaded = function(success) {
    if (!success && this.onThumbnailError_) {
      this.onThumbnailError_(url);
    }
    callback();
  }.bind(this);
  this.metadataCache_.get(url, Gallery.METADATA_TYPE,
      function(metadata) { tile.load(metadata, onImageLoaded) });
};

/**
 * Reload all tiles.
 */
Mosaic.prototype.reload = function() {
  this.layoutModel_.reset_();
  this.tiles_.forEach(function(t) { t.markUnloaded() });
  this.loadTiles_(this.tiles_);
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
 * @param {number=} opt_delay Delay in ms.
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
  this.layoutModel_.setViewportSize(this.clientWidth, this.clientHeight -
      (Mosaic.Layout.PADDING_TOP + Mosaic.Layout.PADDING_BOTTOM));
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
  if (!this.suppressHovering_)
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
  this.layoutModel_.invalidateFromTile_(index);

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

  this.layoutModel_.invalidateFromTile_(index);
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
  return this.tiles_.length < 100;
};

/**
 * Show the mosaic.
 */
Mosaic.prototype.show = function() {
  var duration = ImageView.MODE_TRANSITION_DURATION;
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
 * @param {boolean=} opt_instant True of the transition should be instant.
 */
Mosaic.prototype.transform = function(tileRect, imageRect, opt_instant) {
  if (opt_instant) {
    this.style.webkitTransitionDuration = '0';
  } else {
    this.style.webkitTransitionDuration =
        ImageView.MODE_TRANSITION_DURATION + 'ms';
  }

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

/** @override */
Mosaic.SelectionController.prototype.getLastIndex = function() {
  return this.layoutModel_.getLaidOutTileCount() - 1;
};

/** @override */
Mosaic.SelectionController.prototype.getIndexBefore = function(index) {
  return this.layoutModel_.getHorizontalAdjacentIndex(index, -1);
};

/** @override */
Mosaic.SelectionController.prototype.getIndexAfter = function(index) {
  return this.layoutModel_.getHorizontalAdjacentIndex(index, 1);
};

/** @override */
Mosaic.SelectionController.prototype.getIndexAbove = function(index) {
  return this.layoutModel_.getVerticalAdjacentIndex(index, -1);
};

/** @override */
Mosaic.SelectionController.prototype.getIndexBelow = function(index) {
  return this.layoutModel_.getVerticalAdjacentIndex(index, 1);
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Mosaic layout.
 *
 * @param {string=} opt_mode Layout mode.
 * @param {Mosaic.Density=} opt_maxDensity Layout density.
 * @constructor
 */
Mosaic.Layout = function(opt_mode, opt_maxDensity) {
  this.mode_ = opt_mode || Mosaic.Layout.MODE_TENTATIVE;
  this.maxDensity_ = opt_maxDensity || Mosaic.Density.createHighest();
  this.reset_();
};

/**
 * Blank space at the top of the mosaic element. We do not do that in CSS
 * to make transition effects easier.
 */
Mosaic.Layout.PADDING_TOP = 50;

/**
 * Blank space at the bottom of the mosaic element.
 */
Mosaic.Layout.PADDING_BOTTOM = 50;

/**
 * Horizontal and vertical spacing between images. Should be kept in sync
 * with the style of .mosaic-item in gallery.css (= 2 * ( 4 + 1))
 */
Mosaic.Layout.SPACING = 10;

/**
 * Margin for scrolling using keyboard. Distance between a selected tile
 * and window border.
 */
Mosaic.Layout.SCROLL_MARGIN = 30;

/**
 * Layout mode: commit to DOM immediately.
 */
Mosaic.Layout.MODE_FINAL = 'final';

/**
 * Layout mode: do not commit layout to DOM until it is complete or the viewport
 * overflows.
 */
Mosaic.Layout.MODE_TENTATIVE = 'tentative';

/**
 * Layout mode: never commit layout to DOM.
 */
Mosaic.Layout.MODE_DRY_RUN = 'dry_run';

/**
 * Reset the layout.
 *
 * @private
 */
Mosaic.Layout.prototype.reset_ = function() {
  this.columns_ = [];
  this.newColumn_ = null;
  this.density_ = Mosaic.Density.createLowest();
  if (this.mode_ != Mosaic.Layout.MODE_DRY_RUN)  // DRY_RUN is sticky.
    this.mode_ = Mosaic.Layout.MODE_TENTATIVE;
};

/**
 * @param {number} width Viewport width.
 * @param {number} height Viewport height.
 */
Mosaic.Layout.prototype.setViewportSize = function(width, height) {
  this.viewportWidth_ = width;
  this.viewportHeight_ = height;
  this.reset_();
};

/**
 * @return {number} Total width of the layout.
 */
Mosaic.Layout.prototype.getWidth = function() {
  var lastColumn = this.getLastColumn_();
  return lastColumn ? lastColumn.getRight() : 0;
};

/**
 * @return {number} Total height of the layout.
 */
Mosaic.Layout.prototype.getHeight = function() {
  var firstColumn = this.columns_[0];
  return firstColumn ? firstColumn.getHeight() : 0;
};

/**
 * @return {Array.<Mosaic.Tile>} All tiles in the layout.
 */
Mosaic.Layout.prototype.getTiles = function() {
  return Array.prototype.concat.apply([],
      this.columns_.map(function(c) { return c.getTiles() }));
};

/**
 * @return {number} Total number of tiles added to the layout.
 */
Mosaic.Layout.prototype.getTileCount = function() {
  return this.getLaidOutTileCount() +
      (this.newColumn_ ? this.newColumn_.getTileCount() : 0);
};

/**
 * @return {Mosaic.Column} The last column or null for empty layout.
 * @private
 */
Mosaic.Layout.prototype.getLastColumn_ = function() {
  return this.columns_.length ? this.columns_[this.columns_.length - 1] : null;
};

/**
 * @return {number} Total number of tiles in completed columns.
 */
Mosaic.Layout.prototype.getLaidOutTileCount = function() {
  var lastColumn = this.getLastColumn_();
  return lastColumn ? lastColumn.getNextTileIndex() : 0;
};

/**
 * Add a tile to the layout.
 *
 * @param {Mosaic.Tile} tile The tile to be added.
 * @param {boolean} isLast True if this tile is the last.
 */
Mosaic.Layout.prototype.add = function(tile, isLast) {
  var layoutQueue = [tile];

  // There are two levels of backtracking in the layout algorithm.
  // |Mosaic.Layout.density_| tracks the state of the 'global' backtracking
  // which aims to use as much of the viewport space as possible.
  // It starts with the lowest density and increases it until the layout
  // fits into the viewport. If it does not fit even at the highest density,
  // the layout continues with the highest density.
  //
  // |Mosaic.Column.density_| tracks the state of the 'local' backtracking
  // which aims to avoid producing unnaturally looking columns.
  // It starts with the current global density and decreases it until the column
  // looks nice.

  while (layoutQueue.length) {
    if (!this.newColumn_) {
      var lastColumn = this.getLastColumn_();
      this.newColumn_ = new Mosaic.Column(
          this.columns_.length,
          lastColumn ? lastColumn.getNextRowIndex() : 0,
          lastColumn ? lastColumn.getNextTileIndex() : 0,
          lastColumn ? lastColumn.getRight() : 0,
          this.viewportHeight_,
          this.density_.clone());
    }

    this.newColumn_.add(layoutQueue.shift());

    var isFinalColumn = isLast && !layoutQueue.length;

    if (!this.newColumn_.prepareLayout(isFinalColumn))
      continue; // Column is incomplete.

    if (this.newColumn_.isSuboptimal()) {
      layoutQueue = this.newColumn_.getTiles().concat(layoutQueue);
      this.newColumn_.retryWithLowerDensity();
      continue;
    }

    this.columns_.push(this.newColumn_);
    this.newColumn_ = null;

    if (this.mode_ == Mosaic.Layout.MODE_FINAL) {
      this.getLastColumn_().layout();
      continue;
    }

    if (this.getWidth() > this.viewportWidth_) {
      // Viewport completely filled.
      if (this.density_.equals(this.maxDensity_)) {
        // Max density reached, commit if tentative, just continue if dry run.
        if (this.mode_ == Mosaic.Layout.MODE_TENTATIVE)
          this.commit_();
        continue;
      }

      // Rollback the entire layout, retry with higher density.
      layoutQueue = this.getTiles().concat(layoutQueue);
      this.columns_ = [];
      this.density_.increase();
      continue;
    }

    if (isFinalColumn && this.mode_ == Mosaic.Layout.MODE_TENTATIVE) {
      // The complete tentative layout fits into the viewport.
      var stretched = this.findHorizontalLayout_();
      if (stretched)
        this.columns_ = stretched.columns_;
      // Center the layout in the viewport and commit.
      this.commit_((this.viewportWidth_ - this.getWidth()) / 2,
                   (this.viewportHeight_ - this.getHeight()) / 2);
    }
  }
};

/**
 * Commit the tentative layout.
 *
 * @param {number=} opt_offsetX Horizontal offset.
 * @param {number=} opt_offsetY Vertical offset.
 * @private
 */
Mosaic.Layout.prototype.commit_ = function(opt_offsetX, opt_offsetY) {
  console.assert(this.mode_ != Mosaic.Layout.MODE_FINAL,
      'Did not expect final layout');
  for (var i = 0; i != this.columns_.length; i++) {
    this.columns_[i].layout(opt_offsetX, opt_offsetY);
  }
  this.mode_ = Mosaic.Layout.MODE_FINAL;
};

/**
 * Find the most horizontally stretched layout built from the same tiles.
 *
 * The main layout algorithm fills the entire available viewport height.
 * If there is too few tiles this results in a layout that is unnaturally
 * stretched in the vertical direction.
 *
 * This method tries a number of smaller heights and returns the most
 * horizontally stretched layout that still fits into the viewport.
 *
 * @return {Mosaic.Layout} A horizontally stretched layout.
 * @private
 */
Mosaic.Layout.prototype.findHorizontalLayout_ = function() {
  // If the layout aspect ratio is not dramatically different from
  // the viewport aspect ratio then there is no need to optimize.
  if (this.getWidth() / this.getHeight() >
      this.viewportWidth_ / this.viewportHeight_ * 0.9)
    return null;

  var tiles = this.getTiles();
  if (tiles.length == 1)
    return null;  // Single tile layout is always the same.

  var tileHeights = tiles.map(function(t) { return t.getMaxContentHeight() });
  var minTileHeight = Math.min.apply(null, tileHeights);

  for (var h = minTileHeight; h < this.viewportHeight_; h += minTileHeight) {
    var layout = new Mosaic.Layout(
        Mosaic.Layout.MODE_DRY_RUN, this.density_.clone());
    layout.setViewportSize(this.viewportWidth_, h);
    for (var t = 0; t != tiles.length; t++)
      layout.add(tiles[t], t + 1 == tiles.length);

    if (layout.getWidth() <= this.viewportWidth_)
      return layout;
  }

  return null;
};

/**
 * Invalidate the layout after the given tile was modified (added, deleted or
 * changed dimensions).
 *
 * @param {number} index Tile index.
 * @private
 */
Mosaic.Layout.prototype.invalidateFromTile_ = function(index) {
  var columnIndex = this.getColumnIndexByTile_(index);
  if (columnIndex < 0)
    return; // Index not in the layout, probably already invalidated.

  if (this.columns_[columnIndex].getLeft() >= this.viewportWidth_) {
    // The columns to the right cover the entire viewport width, so there is no
    // chance that the modified layout would fit into the viewport.
    // No point in restarting the entire layout, keep the columns to the right.
    console.assert(this.mode_ == Mosaic.Layout.MODE_FINAL,
        'Expected FINAL layout mode');
    this.columns_ = this.columns_.slice(0, columnIndex);
    this.newColumn_ = null;
  } else {
    // There is a chance that the modified layout would fit into the viewport.
    this.reset_();
    this.mode_ = Mosaic.Layout.MODE_TENTATIVE;
  }
};

/**
 * Get the index of the tile to the left or to the right from the given tile.
 *
 * @param {number} index Tile index.
 * @param {number} direction -1 for left, 1 for right.
 * @return {number} Adjacent tile index.
 */
Mosaic.Layout.prototype.getHorizontalAdjacentIndex = function(
    index, direction) {
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
 * Get the index of the tile to the top or to the bottom from the given tile.
 *
 * @param {number} index Tile index.
 * @param {number} direction -1 for above, 1 for below.
 * @return {number} Adjacent tile index.
 */
Mosaic.Layout.prototype.getVerticalAdjacentIndex = function(
    index, direction) {
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

  // Find the first item in the next row, or the last item in the previous row.
  var adjacentRowNeighbourIndex =
      row.getEdgeTileIndex_(direction) + direction;

  if (adjacentRowNeighbourIndex < 0 ||
      adjacentRowNeighbourIndex > this.getTileCount() - 1)
    return -1;

  if (!this.columns_[column].hasTile(adjacentRowNeighbourIndex)) {
    // It is not in the current column, so return it.
    return adjacentRowNeighbourIndex;
  } else {
    // It is in the current column, so we have to find optically the closest
    // tile in the adjacent row.
    var adjacentRow = this.columns_[column].getRowByTileIndex(
        adjacentRowNeighbourIndex);
    var previousTileCenterX = row.getTileByIndex(index).getCenterX();

    // Find the closest one.
    var closestIndex = -1;
    var closestDistance;
    var adjacentRowTiles = adjacentRow.getTiles();
    for (var t = 0; t != adjacentRowTiles.length; t++) {
      var distance =
          Math.abs(adjacentRowTiles[t].getCenterX() - previousTileCenterX);
      if (closestIndex == -1 || distance < closestDistance) {
        closestIndex = adjacentRow.getEdgeTileIndex_(-1) + t;
        closestDistance = distance;
      }
    }
    return closestIndex;
  }
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
 * Representation of the layout density.
 *
 * @param {number} horizontal Horizontal density, number tiles per row.
 * @param {number} vertical Vertical density, frequency of rows forced to
 *   contain a single tile.
 * @constructor
 */
Mosaic.Density = function(horizontal, vertical) {
  this.horizontal = horizontal;
  this.vertical = vertical;
};

/**
 * Minimal horizontal density (tiles per row).
 */
Mosaic.Density.MIN_HORIZONTAL = 1;

/**
 * Minimal horizontal density (tiles per row).
 */
Mosaic.Density.MAX_HORIZONTAL = 3;

/**
 * Minimal vertical density: force 1 out of 2 rows to containt a single tile.
 */
Mosaic.Density.MIN_VERTICAL = 2;

/**
 * Maximal vertical density: force 1 out of 3 rows to containt a single tile.
 */
Mosaic.Density.MAX_VERTICAL = 3;

/**
 * @return {Mosaic.Density} Lowest density.
 */
Mosaic.Density.createLowest = function() {
  return new Mosaic.Density(
      Mosaic.Density.MIN_HORIZONTAL,
      Mosaic.Density.MIN_VERTICAL /* ignored when horizontal is at min */);
};

/**
 * @return {Mosaic.Density} Highest density.
 */
Mosaic.Density.createHighest = function() {
  return new Mosaic.Density(
      Mosaic.Density.MAX_HORIZONTAL,
      Mosaic.Density.MAX_VERTICAL);
};

/**
 * @return {Mosaic.Density} A clone of this density object.
 */
Mosaic.Density.prototype.clone = function() {
  return new Mosaic.Density(this.horizontal, this.vertical);
};

/**
 * @param {Mosaic.Density} that The other object.
 * @return {boolean} True if equal.
 */
Mosaic.Density.prototype.equals = function(that) {
  return this.horizontal == that.horizontal &&
         this.vertical == that.vertical;
};

/**
 * Increase the density to the next level.
 */
Mosaic.Density.prototype.increase = function() {
  if (this.horizontal == Mosaic.Density.MIN_HORIZONTAL ||
      this.vertical == Mosaic.Density.MAX_VERTICAL) {
    console.assert(this.horizontal < Mosaic.Density.MAX_HORIZONTAL);
    this.horizontal++;
    this.vertical = Mosaic.Density.MIN_VERTICAL;
  } else {
    this.vertical++;
  }
};

/**
 * Decrease horizontal density.
 */
Mosaic.Density.prototype.decreaseHorizontal = function() {
  console.assert(this.horizontal > Mosaic.Density.MIN_HORIZONTAL);
  this.horizontal--;
};

/**
 * @param {number} tileCount Number of tiles in the row.
 * @param {number} rowIndex Global row index.
 * @return {boolean} True if the row is complete.
 */
Mosaic.Density.prototype.isRowComplete = function(tileCount, rowIndex) {
  return (tileCount == this.horizontal) || (rowIndex % this.vertical) == 0;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * A column in a mosaic layout. Contains rows.
 *
 * @param {number} index Column index.
 * @param {number} firstRowIndex Global row index.
 * @param {number} firstTileIndex Index of the first tile in the column.
 * @param {number} left Left edge coordinate.
 * @param {number} maxHeight Maximum height.
 * @param {Mosaic.Density} density Layout density.
 * @constructor
 */
Mosaic.Column = function(index, firstRowIndex, firstTileIndex, left, maxHeight,
                         density) {
  this.index_ = index;
  this.firstRowIndex_ = firstRowIndex;
  this.firstTileIndex_ = firstTileIndex;
  this.left_ = left;
  this.maxHeight_ = maxHeight;
  this.density_ = density;

  this.reset_();
};

/**
 * Reset the layout.
 * @private
 */
Mosaic.Column.prototype.reset_ = function() {
  this.tiles_ = [];
  this.rows_ = [];
  this.newRow_ = null;
};

/**
 * @return {number} Number of tiles in the column.
 */
Mosaic.Column.prototype.getTileCount = function() { return this.tiles_.length };

/**
 * @return {number} Index of the last tile + 1.
 */
Mosaic.Column.prototype.getNextTileIndex = function() {
  return this.firstTileIndex_ + this.getTileCount();
};

/**
 * @return {number} Global index of the last row + 1.
 */
Mosaic.Column.prototype.getNextRowIndex = function() {
  return this.firstRowIndex_ + this.rows_.length;
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
  var rowIndex = this.getNextRowIndex();

  if (!this.newRow_)
     this.newRow_ = new Mosaic.Row(this.getNextTileIndex());

  this.tiles_.push(tile);
  this.newRow_.add(tile);

  if (this.density_.isRowComplete(this.newRow_.getTileCount(), rowIndex)) {
    this.rows_.push(this.newRow_);
    this.newRow_ = null;
  }
};

/**
 * Prepare the column layout.
 *
 * @param {boolean=} opt_force True if the layout must be performed even for an
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

  this.height_ = 0;

  this.rowHeights_ = [];
  for (var r = 0; r != this.rows_.length; r++) {
    var rowHeight = this.rows_[r].getHeightForWidth(this.width_);
    this.height_ += rowHeight;
    this.rowHeights_.push(rowHeight);
  }

  var overflow = this.height_ / this.maxHeight_;
  if (!opt_force && (overflow < 1))
    return false;

  if (overflow > 1) {
    // Scale down the column width and height.
    this.width_ = Math.round(this.width_ / overflow);
    this.height_ = this.maxHeight_;
    Mosaic.Layout.rescaleSizesToNewTotal(this.rowHeights_, this.maxHeight_);
  }

  return true;
};

/**
 * Retry the column layout with less tiles per row.
 */
Mosaic.Column.prototype.retryWithLowerDensity = function() {
  this.density_.decreaseHorizontal();
  this.reset_();
};

/**
 * @return {number} Column left edge coordinate.
 */
Mosaic.Column.prototype.getLeft = function() { return this.left_ };

/**
 * @return {number} Column right edge coordinate after the layout.
 */
Mosaic.Column.prototype.getRight = function() {
  return this.left_ + this.width_;
};

/**
 * @return {number} Column height after the layout.
 */
Mosaic.Column.prototype.getHeight = function() { return this.height_ };

/**
 * Perform the column layout.
 * @param {number=} opt_offsetX Horizontal offset.
 * @param {number=} opt_offsetY Vertical offset.
 */
Mosaic.Column.prototype.layout = function(opt_offsetX, opt_offsetY) {
  opt_offsetX = opt_offsetX || 0;
  opt_offsetY = opt_offsetY || 0;
  var rowTop = Mosaic.Layout.PADDING_TOP;
  for (var r = 0; r != this.rows_.length; r++) {
    this.rows_[r].layout(
        opt_offsetX + this.left_,
        opt_offsetY + rowTop,
        this.width_,
        this.rowHeights_[r]);
    rowTop += this.rowHeights_[r];
  }
};

/**
 * Check if the column layout is too ugly to be displayed.
 *
 * @return {boolean} True if the layout is suboptimal.
 */
Mosaic.Column.prototype.isSuboptimal = function() {
  var tileCounts =
      this.rows_.map(function(row) { return row.getTileCount() });

  var maxTileCount = Math.max.apply(null, tileCounts);
  if (maxTileCount == 1)
    return false;  // Every row has exactly 1 tile, as optimal as it gets.

  var sizes =
      this.tiles_.map(function(tile) { return tile.getMaxContentHeight() });

  // Ugly layout #1: all images are small and some are one the same row.
  var allSmall = Math.max.apply(null, sizes) <= Mosaic.Tile.SMALL_IMAGE_SIZE;
  if (allSmall)
    return true;

  // Ugly layout #2: all images are large and none occupies an entire row.
  var allLarge = Math.min.apply(null, sizes) > Mosaic.Tile.SMALL_IMAGE_SIZE;
  var allCombined = Math.min.apply(null, tileCounts) != 1;
  if (allLarge && allCombined)
    return true;

  // Ugly layout #3: some rows have too many tiles for the resulting width.
  if (this.width_ / maxTileCount < 100)
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
 * @param {Mosaic.Tile} tile The tile to add.
 */
Mosaic.Row.prototype.add = function(tile) {
  console.assert(this.getTileCount() < Mosaic.Density.MAX_HORIZONTAL);
  this.tiles_.push(tile);
};

/**
 * @return {Array.<Mosaic.Tile>} Array of tiles in the row.
 */
Mosaic.Row.prototype.getTiles = function() { return this.tiles_ };

/**
 * Get a tile by index.
 * @param {number} index Tile index.
 * @return {Mosaic.Tile} Requested tile or null if not found.
 */
Mosaic.Row.prototype.getTileByIndex = function(index) {
  if (!this.hasTile(index))
    return null;
  return this.tiles_[index - this.firstTileIndex_];
};

/**
 *
 * @return {number} Number of tiles in the row.
 */
Mosaic.Row.prototype.getTileCount = function() { return this.tiles_.length };

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
 * Mark the tile as not loaded to prevent it from participating in the layout.
 */
Mosaic.Tile.prototype.markUnloaded = function() {
  this.maxContentHeight_ = 0;
};

/**
 * Load the thumbnail image into the tile.
 *
 * @param {Object} metadata Metadata object.
 * @param {function} callback Completion callback.
 */
Mosaic.Tile.prototype.load = function(metadata, callback) {
  this.markUnloaded();
  this.left_ = null;  // Mark as not laid out.

  this.thumbnailLoader_ = new ThumbnailLoader(this.getItem().getUrl(),
                                              ThumbnailLoader.LoaderType.CANVAS,
                                              metadata);

  this.thumbnailLoader_.loadDetachedImage(function(success) {
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

    callback(success);
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
    this.scrollIntoView(false);

  this.thumbnailLoader_.attachImage(this.wrapper_,
                                    ThumbnailLoader.FillMode.FILL);
};

/**
 * If the tile is not fully visible scroll the parent to make it fully visible.
 * @param {boolean=} opt_animated True, if scroll should be animated,
 *     default: true.
 */
Mosaic.Tile.prototype.scrollIntoView = function(opt_animated) {
  if (this.left_ == null)  // Not laid out.
    return;

  var targetPosition;
  var tileLeft = this.left_ - Mosaic.Layout.SCROLL_MARGIN;
  if (tileLeft < this.container_.scrollLeft) {
    targetPosition = tileLeft;
  } else {
    var tileRight = this.left_ + this.width_ + Mosaic.Layout.SCROLL_MARGIN;
    var scrollRight = this.container_.scrollLeft + this.container_.clientWidth;
    if (tileRight > scrollRight)
      targetPosition = tileRight - this.container_.clientWidth;
  }

  if (targetPosition) {
    if (opt_animated === false)
      this.container_.scrollLeft = targetPosition;
    else
      this.container_.animatedScrollTo(targetPosition);
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

/**
 * @return {number} X coordinate of the tile center.
 */
Mosaic.Tile.prototype.getCenterX = function() {
  return this.left_ + Math.round(this.width_ / 2);
};
