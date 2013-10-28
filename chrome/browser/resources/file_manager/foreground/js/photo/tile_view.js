// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tile view displays images/videos tiles.
 *
 * @param {HTMLDocument} document Document.
 * @param {function(TailBox, callback)} prepareBox This function should provide
 *     the passed box with width and height properties of the image.
 * @param {function(TailBox, callback)} loadBox This function should display
 *     the image in the box respecting clientWidth and clientHeight.
 * @constructor
 */
function TileView(document, prepareBox, loadBox) {
  var self = document.createElement('div');
  TileView.decorate(self, prepareBox, loadBox);
  return self;
}

TileView.prototype = { __proto__: HTMLDivElement.prototype };

/**
 * The number of boxes updated at once after loading.
 */
TileView.LOAD_CHUNK = 10;

/**
 * The margin between the boxes (in pixels).
 */
TileView.MARGIN = 10;

/**
 * The delay between loading of two consecutive images.
 */
TileView.LOAD_DELAY = 100;

/**
 * @param {HTMLDivElement} self Element to decorate.
 * @param {function(TailBox, callback)} prepareBox See constructor.
 * @param {function(TailBox, callback)} loadBox See constructor.
 */
TileView.decorate = function(self, prepareBox, loadBox) {
  self.__proto__ = TileView.prototype;
  self.classList.add('tile-view');
  self.prepareBox_ = prepareBox;
  self.loadBox_ = loadBox;
};

/**
 * Load and display media entries.
 * @param {Array.<FileEntry>} entries Entries list.
 */
TileView.prototype.load = function(entries) {
  this.boxes_ = [];

  /**
   * The number of boxes for which the image size is already known.
   */
  this.preparedCount_ = 0;

  /**
   * The number of boxes already displaying the image.
   */
  this.loadedCount_ = 0;

  for (var index = 0; index < entries.length; index++) {
    var box = new TileBox(this, entries[index]);
    box.index = index;
    this.boxes_.push(box);
    this.prepareBox_(box, this.onBoxPrepared_.bind(this, box));
  }

  this.redraw();
};

/**
 * Redraws everything.
 */
TileView.prototype.redraw = function() {
  // TODO(dgozman): if we decide to support resize or virtual scrolling,
  // we should save the chosen position for ready boxes, so they will not
  // move around.

  this.cellSize_ = Math.floor((this.clientHeight - 3 * TileView.MARGIN) / 2);
  this.textContent = '';
  for (var index = 0; index < this.boxes_.length; index++) {
    this.appendChild(this.boxes_[index]);
  }
  this.repositionBoxes_(0);
};

/**
 * This function sets positions for boxes.
 *
 * To do this we keep a 2x4 array of cells marked busy or empty.
 * When trying to put the next box, we choose a pattern (horizontal, vertical,
 * square, etc.) which fits into the empty cells and place an image there.
 * The preferred pattern has the same orientation as image itself. Images
 * with unknown size are always shown in 1x1 cell.
 *
 * @param {number} from First index.
 * @private
 */
TileView.prototype.repositionBoxes_ = function(from) {

  var cellSize = this.cellSize_;
  var margin = TileView.MARGIN;
  var baseColAndEmpty = this.getBaseColAndEmpty_();

  // |empty| is a 2x4 array of busy/empty cells.
  var empty = baseColAndEmpty.empty;

  // |baseCol| is the (tileview-wide) number of first column in |empty| array.
  var baseCol = baseColAndEmpty.baseCol;

  for (var index = from; index < this.boxes_.length; index++) {
    while (!empty[0][0] && !empty[1][0]) {
      // Skip the full columns at the start.
      empty[0].shift();
      empty[0].push(true);
      empty[1].shift();
      empty[1].push(true);
      baseCol++;
    }
    // Here we always have an empty cell in the first column fo |empty| array,
    // and |baseCol| is the column number of it.

    var box = this.boxes_[index];
    var imageWidth = box.width || 0;
    var imageHeight = box.height || 0;

    // Possible positions of the box:
    //   p - the probability of this pattern to be used;
    //   w - the width of resulting image (in columns);
    //   h - the height of resulting image (in rows);
    //   allowed - whether this pattern is allowed for this particular box.
    var patterns = [
      {p: 0.2, w: 2, h: 2, allowed: index < this.preparedCount_},
      {p: 0.6, w: 1, h: 2, allowed: imageHeight > imageWidth &&
                                    index < this.preparedCount_},
      {p: 0.3, w: 2, h: 1, allowed: imageHeight < imageWidth &&
                                    index < this.preparedCount_},
      {p: 1.0, w: 1, h: 1, allowed: true} // Every image can be shown as 1x1.
    ];

    // The origin point is top-left empty cell, which must be in the
    // first column.
    var col = 0;
    var row = empty[0][0] ? 0 : 1;

    for (var pIndex = 0; pIndex < patterns.length; pIndex++) {
      var pattern = patterns[pIndex];
      if (Math.random() > pattern.p || !pattern.allowed) continue;
      if (!this.canUsePattern_(empty, row, col, pattern)) continue;

      // Found a pattern to use.
      box.rect.row = row;
      box.rect.col = col + baseCol;
      box.rect.width = pattern.w;
      box.rect.height = pattern.h;

      // Now mark the cells as busy and stop.
      this.usePattern_(empty, row, col, pattern);
      break;
    }

    box.setPositionFromRect(margin, cellSize);
  }
};

/**
 * @param {number} from Starting index.
 * @return {Object} An object containing the array of cells marked empty/busy
 *   and a base (left one) column number.
 * @private
 */
TileView.prototype.getBaseColAndEmpty_ = function(from) {
  // 2x4 array indicating whether the place is empty or not.
  var empty = [[true, true, true, true], [true, true, true, true]];
  var baseCol = 0;

  if (from > 0) {
    baseCol = this.boxes_[from - 1].rect.col;
    if (from > 1) {
      baseCol = Math.min(baseCol, this.boxes_[from - 2].rect.col);
    }

    for (var b = from - 2; b < from; b++) {
      if (b < 0) continue;
      var rect = this.boxes_[b].rect;
      for (var i = 0; i < rect.height; i++) {
        for (var j = 0; j < rect.width; j++) {
          empty[i + rect.row][j + rect.col - baseCol] = false;
        }
      }
    }
  }

  return {empty: empty, baseCol: baseCol};
};

/**
 * @param {Array} empty The empty/busy cells array.
 * @param {number} row The origin row.
 * @param {number} col The origin column.
 * @param {Object} pattern The pattern (see |repositionBoxes_|).
 * @return {boolean} Whether the pattern may be used at this origin.
 * @private
 */
TileView.prototype.canUsePattern_ = function(empty, row, col, pattern) {
  if (row + pattern.h > 2 || col + pattern.w > 4)
    return false;

  var can = true;
  for (var r = 0; r < pattern.h; r++) {
    for (var c = 0; c < pattern.w; c++) {
      can = can && empty[row + r][col + c];
    }
  }
  return can;
};

/**
 * Marks pattern's cells as busy.
 * @param {Array} empty The empty/busy cells array.
 * @param {number} row The origin row.
 * @param {number} col The origin column.
 * @param {Object} pattern The pattern (see |repositionBoxes_|).
 * @private
 */
TileView.prototype.usePattern_ = function(empty, row, col, pattern) {
  for (var r = 0; r < pattern.h; r++) {
    for (var c = 0; c < pattern.w; c++) {
      empty[row + r][col + c] = false;
    }
  }
};

/**
 * Called when box is ready.
 * @param {TileBox} box The box.
 * @private
 */
TileView.prototype.onBoxPrepared_ = function(box) {
  box.ready = true;
  var to = this.preparedCount_;
  while (to < this.boxes_.length && this.boxes_[to].ready) {
    to++;
  }

  if (to >= Math.min(this.preparedCount_ + TileView.LOAD_CHUNK,
                     this.boxes_.length)) {
    var last = this.preparedCount_;
    this.preparedCount_ = to;
    this.repositionBoxes_(last);

    if (this.loadedCount_ == last) {
      // All previously prepared boxes have been loaded - start the next one.
      var nextBox = this.boxes_[this.loadedCount_];
      setTimeout(this.loadBox_, TileView.LOAD_DELAY,
                 nextBox, this.onBoxLoaded.bind(this, nextBox));
    }
  }
};

/**
 * Called when box is loaded.
 * @param {TileBox} box The box.
 */
TileView.prototype.onBoxLoaded = function(box) {
  if (this.loadedCount_ != box.index)
    console.error('inconsistent loadedCount');
  this.loadedCount_ = box.index + 1;

  var nextIndex = box.index + 1;
  if (nextIndex < this.preparedCount_) {
    var nextBox = this.boxes_[nextIndex];
    setTimeout(this.loadBox_, TileView.LOAD_DELAY,
               nextBox, this.onBoxLoaded.bind(this, nextBox));
  }
};



/**
 * Container for functions to work with local TileView.
 */
TileView.local = {};

/**
 * Decorates a TileView to show local files.
 * @param {HTMLDivElement} view The view.
 * @param {MetadataCache} metadataCache Metadata cache.
 */
TileView.local.decorate = function(view, metadataCache) {
  TileView.decorate(view, TileView.local.prepareBox, TileView.local.loadBox);
  view.metadataCache = metadataCache;
};

/**
 * Prepares image for local tile view box.
 * @param {TileBox} box The box.
 * @param {function} callback The callback.
 */
TileView.local.prepareBox = function(box, callback) {
  box.view_.metadataCache.get(box.entry, 'media', function(media) {
    if (!media) {
      box.width = 0;
      box.height = 0;
      box.imageTransform = null;
    } else {
      if (media.imageTransform && media.imageTransform.rotate90 % 2 == 1) {
        box.width = media.height;
        box.height = media.width;
      } else {
        box.width = media.width;
        box.height = media.height;
      }
      box.imageTransform = media.imageTransform;
    }

    callback();
  });
};

/**
 * Loads the image for local tile view box.
 * @param {TileBox} box The box.
 * @param {function} callback The callback.
 */
TileView.local.loadBox = function(box, callback) {
  var onLoaded = function(fullCanvas) {
    try {
      var canvas = box.ownerDocument.createElement('canvas');
      canvas.width = box.clientWidth;
      canvas.height = box.clientHeight;
      var context = canvas.getContext('2d');
      context.drawImage(fullCanvas, 0, 0, canvas.width, canvas.height);
      box.appendChild(canvas);
    } catch (e) {
      // TODO(dgozman): classify possible exceptions here and reraise others.
    }
    callback();
  };

  var transformFetcher = function(url, onFetched) {
    onFetched(box.imageTransform);
  };

  var imageLoader = new ImageUtil.ImageLoader(box.ownerDocument);
  imageLoader.load(box.entry.toURL(), transformFetcher, onLoaded);
};



/**
 * Container for functions to work with drive TileView.
 */
TileView.drive = {};

/**
 * Decorates a TileView to show drive files.
 * @param {HTMLDivElement} view The view.
 * @param {MetadataCache} metadataCache Metadata cache.
 */
TileView.drive.decorate = function(view, metadataCache) {
  TileView.decorate(view, TileView.drive.prepareBox, TileView.drive.loadBox);
  view.metadataCache = metadataCache;
};

/**
 * Prepares image for drive tile view box.
 * @param {TileBox} box The box.
 * @param {function} callback The callback.
 */
TileView.drive.prepareBox = function(box, callback) {
  box.view_.metadataCache.get(box.entry, 'thumbnail', function(thumbnail) {
    if (!thumbnail) {
      box.width = 0;
      box.height = 0;
      callback();
      return;
    }

    // TODO(dgozman): remove this hack if we ask for larger thumbnails in
    // drive code.
    var thumbnailUrl = thumbnail.url.replace(/240$/, '512');

    box.image = new Image();
    box.image.onload = function(e) {
      box.width = box.image.width;
      box.height = box.image.height;
      callback();
    };
    box.image.onerror = function() {
      box.image = null;
      callback();
    };
    box.image.src = thumbnailUrl;
  });
};

/**
 * Loads the image for drive tile view box.
 * @param {TileBox} box The box.
 * @param {function} callback The callback.
 */
TileView.drive.loadBox = function(box, callback) {
  box.appendChild(box.image);
  callback();
};




/**
 * Tile box is a part of tile view.
 * @param {TailView} view The parent view.
 * @param {Entry} entry Image file entry.
 * @constructor
 */
function TileBox(view, entry) {
  var self = view.ownerDocument.createElement('div');
  TileBox.decorate(self, view, entry);
  return self;
}

TileBox.prototype = { __proto__: HTMLDivElement.prototype };

/**
 * @param {HTMLDivElement} self Element to decorate.
 * @param {TailView} view The parent view.
 * @param {Entry} entry Image file entry.
 */
TileBox.decorate = function(self, view, entry) {
  self.__proto__ = TileBox.prototype;
  self.classList.add('tile-box');

  self.view_ = view;
  self.entry = entry;

  self.ready = false;
  self.rect = {row: 0, col: 0, width: 0, height: 0};

  self.index = null;
  self.height = null;
  self.width = null;
};

/**
 * Sets box position according to the |rect| property and given sizes.
 * @param {number} margin Margin between cells.
 * @param {number} cellSize The size of one cell.
 * @constructor
 */
TileBox.setPositionFromRect = function(margin, cellSize) {
  this.style.top = margin + (cellSize + margin) * this.rect.row + 'px';
  this.style.left = margin + (cellSize + margin) * this.rect.col + 'px';
  this.style.height = (cellSize + margin) * this.rect.height - margin + 'px';
  this.style.width = (cellSize + margin) * this.rect.width - margin + 'px';
};
