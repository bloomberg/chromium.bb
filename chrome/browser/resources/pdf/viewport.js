// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Predefined zoom factors to be used when zooming in/out. These are in
 * ascending order.
 */
var ZOOM_FACTORS = [0.25, 0.333, 0.5, 0.666, 0.75, 0.9, 1,
                    1.1, 1.25, 1.5, 1.75, 2, 2.5, 3, 4, 5];

/**
 * Returns the area of the intersection of two rectangles.
 * @param {Object} rect1 the first rect
 * @param {Object} rect2 the second rect
 * @return {number} the area of the intersection of the rects
 */
function getIntersectionArea(rect1, rect2) {
  var xOverlap = Math.max(0,
      Math.min(rect1.x + rect1.width, rect2.x + rect2.width) -
      Math.max(rect1.x, rect2.x));
  var yOverlap = Math.max(0,
      Math.min(rect1.y + rect1.height, rect2.y + rect2.height) -
      Math.max(rect1.y, rect2.y));
  return xOverlap * yOverlap;
}

/**
 * @return {number} width of a scrollbar in pixels
 */
function getScrollbarWidth() {
  var parentDiv = document.createElement('div');
  parentDiv.style.visibility = 'hidden';
  var parentDivWidth = 500;
  parentDiv.style.width = parentDivWidth + 'px';
  document.body.appendChild(parentDiv);
  parentDiv.style.overflow = 'scroll';
  var childDiv = document.createElement('div');
  childDiv.style.width = '100%';
  parentDiv.appendChild(childDiv);
  var childDivWidth = childDiv.offsetWidth;
  parentDiv.parentNode.removeChild(parentDiv);
  return parentDivWidth - childDivWidth;
}

/**
 * Create a new viewport.
 * @param {Window} window the window
 * @param {Object} sizer is the element which represents the size of the
 *     document in the viewport
 * @param {Function} fitToPageEnabledFunction returns true if fit-to-page is
 *     enabled
 * @param {Function} viewportChangedCallback is run when the viewport changes
 */
function Viewport(window,
                  sizer,
                  fitToPageEnabledFunction,
                  viewportChangedCallback) {
  this.window_ = window;
  this.sizer_ = sizer;
  this.fitToPageEnabledFunction_ = fitToPageEnabledFunction;
  this.viewportChangedCallback_ = viewportChangedCallback;
  this.zoom_ = 1;
  this.documentDimensions_ = {};
  this.pageDimensions_ = [];
  this.scrollbarWidth_ = getScrollbarWidth();

  window.addEventListener('scroll', this.updateViewport_.bind(this));
}

Viewport.prototype = {
  /**
   * @private
   * Returns true if the document needs scrollbars at the given zoom level.
   * @param {number} zoom compute whether scrollbars are needed at this zoom
   * @return {Object} with 'x' and 'y' keys which map to bool values
   *     indicating if the horizontal and vertical scrollbars are needed
   *     respectively.
   */
  documentNeedsScrollbars_: function(zoom) {
    return {
      x: this.documentDimensions_.width * zoom > this.window_.innerWidth,
      y: this.documentDimensions_.height * zoom > this.window_.innerHeight
    };
  },

  /**
   * Returns true if the document needs scrollbars at the current zoom level.
   * @return {Object} with 'x' and 'y' keys which map to bool values
   *     indicating if the horizontal and vertical scrollbars are needed
   *     respectively.
   */
  documentHasScrollbars: function() {
    return this.documentNeedsScrollbars_(this.zoom_);
  },

  /**
   * @private
   * Helper function called when the zoomed document size changes.
   */
  contentSizeChanged_: function() {
    this.sizer_.style.width =
        this.documentDimensions_.width * this.zoom_ + 'px';
    this.sizer_.style.height =
        this.documentDimensions_.height * this.zoom_ + 'px';
  },

  /**
   * Sets the zoom of the viewport.
   * @param {number} newZoom the zoom level to zoom to
   */
  setZoom: function(newZoom) {
    var oldZoom = this.zoom_;
    this.zoom_ = newZoom;
    // Record the scroll position (relative to the middle of the window).
    var currentScrollPos = [
      (this.window_.scrollX + this.window_.innerWidth / 2) / oldZoom,
      (this.window_.scrollY + this.window_.innerHeight / 2) / oldZoom
    ];
    this.contentSizeChanged_();
    // Scroll to the scaled scroll position.
    this.window_.scrollTo(
        currentScrollPos[0] * newZoom - this.window_.innerWidth / 2,
        currentScrollPos[1] * newZoom - this.window_.innerHeight / 2);
  },

  /**
   * @private
   * Called when the viewport should be updated.
   */
  updateViewport_: function() {
    var needsScrollbars = this.documentHasScrollbars();
    var page = this.getMostVisiblePage();
    this.viewportChangedCallback_(this.zoom_,
                                  this.window_.pageXOffset,
                                  this.window_.pageYOffset,
                                  this.scrollbarWidth_,
                                  needsScrollbars,
                                  page);
  },

  /**
   * @private
   * Returns a rect representing the current viewport.
   * @return {Object} a rect representing the current viewport.
   */
  getCurrentViewportRect_: function() {
    return {
      x: this.window_.pageXOffset / this.zoom_,
      y: this.window_.pageYOffset / this.zoom_,
      width: this.window_.innerWidth / this.zoom_,
      height: this.window_.innerHeight / this.zoom_,
    };
  },

  /**
   * @private
   * @param {integer} y the y-coordinate to get the page at.
   * @return {integer} the index of a page overlapping the given y-coordinate.
   */
  getPageAtY_: function(y) {
    var min = 0;
    var max = this.pageDimensions_.length - 1;
    while (max >= min) {
      var page = Math.floor(min + ((max - min) / 2));
      // There might be a gap between the pages, in which case use the bottom
      // of the previous page as the top for finding the page.
      var top = 0;
      if (page > 0) {
        top = this.pageDimensions_[page - 1].y +
            this.pageDimensions_[page - 1].height;
      }
      var bottom = this.pageDimensions_[page].y +
          this.pageDimensions_[page].height;

      if (top <= y && bottom > y)
        return page;
      else if (top > y)
        max = page - 1;
      else
        min = page + 1;
    }
    return 0;
  },

  /**
   * Returns the page with the most pixels in the current viewport.
   * @return {int} the index of the most visible page.
   */
  getMostVisiblePage: function() {
    var firstVisiblePage = this.getPageAtY_(this.getCurrentViewportRect_().y);
    var mostVisiblePage = {'number': 0, 'area': 0};
    for (var i = firstVisiblePage; i < this.pageDimensions_.length; i++) {
      var area = getIntersectionArea(this.pageDimensions_[i],
                                     this.getCurrentViewportRect_());
      // If we hit a page with 0 area overlap, we must have gone past the
      // pages visible in the viewport so we can break.
      if (area == 0)
        break;
      if (area > mostVisiblePage.area) {
        mostVisiblePage.area = area;
        mostVisiblePage.number = i;
      }
    }
    return mostVisiblePage.number;
  },

  /**
   * @private
   * Compute the zoom level for fit-to-page or fit-to-width. |pageDimensions| is
   * the dimensions for a given page and if |widthOnly| is true, it indicates
   * that fit-to-page zoom should be computed rather than fit-to-page.
   * @param {Object} pageDimensions the dimensions of a given page
   * @param {boolean} widthOnly a bool indicating whether fit-to-page or
   *     fit-to-width should be computed.
   * @return {number} the zoom to use
   */
  computeFittingZoom_: function(pageDimensions, widthOnly) {
    // First compute the zoom without scrollbars.
    var zoomWidth = this.window_.innerWidth / pageDimensions.width;
    var zoom;
    if (widthOnly) {
      zoom = zoomWidth;
    } else {
      var zoomHeight = this.window_.innerHeight / pageDimensions.height;
      zoom = Math.min(zoomWidth, zoomHeight);
    }
    // Check if there needs to be any scrollbars.
    var needsScrollbars = this.documentNeedsScrollbars_(zoom);

    // If the document fits, just return the zoom.
    if (!needsScrollbars.x && !needsScrollbars.y)
      return zoom;

    var zoomedDimensions = {
      width: this.documentDimensions_.width * zoom,
      height: this.documentDimensions_.height * zoom
    };

    // Check if adding a scrollbar will result in needing the other scrollbar.
    var scrollbarWidth = this.scrollbarWidth_;
    if (needsScrollbars.x &&
        zoomedDimensions.height > this.window_.innerHeight - scrollbarWidth) {
      needsScrollbars.y = true;
    }
    if (needsScrollbars.y &&
        zoomedDimensions.width > this.window_.innerWidth - scrollbarWidth) {
      needsScrollbars.x = true;
    }

    // Compute available window space.
    var windowWithScrollbars = {
      width: this.window_.innerWidth,
      height: this.window_.innerHeight
    };
    if (needsScrollbars.x)
      windowWithScrollbars.height -= scrollbarWidth;
    if (needsScrollbars.y)
      windowWithScrollbars.width -= scrollbarWidth;

    // Recompute the zoom.
    zoomWidth = windowWithScrollbars.width / pageDimensions.width;
    if (widthOnly) {
      zoom = zoomWidth;
    } else {
      var zoomHeight = windowWithScrollbars.height / pageDimensions.height;
      zoom = Math.min(zoomWidth, zoomHeight);
    }
    return zoom;
  },

  /**
   * Zoom the viewport so that the page-width consumes the entire viewport.
   */
  fitToWidth: function() {
    var page = this.getMostVisiblePage();
    this.setZoom(this.computeFittingZoom_(this.pageDimensions_[page], true));
    this.window_.scrollTo(this.pageDimensions_[page].x * this.zoom_,
                          this.window_.scrollY);
    this.updateViewport_();
  },

  /**
   * Zoom the viewport so that a page consumes the entire viewport. Also scrolls
   * to the top of the most visible page.
   */
  fitToPage: function() {
    var page = this.getMostVisiblePage();
    this.setZoom(this.computeFittingZoom_(this.pageDimensions_[page], false));
    this.window_.scrollTo(this.pageDimensions_[page].x * this.zoom_,
                          this.pageDimensions_[page].y * this.zoom_);
    this.updateViewport_();
  },

  /**
   * Zoom out to the next predefined zoom level.
   */
  zoomOut: function() {
    var nextZoom = ZOOM_FACTORS[0];
    for (var i = 0; i < ZOOM_FACTORS.length; i++) {
      if (ZOOM_FACTORS[i] < this.zoom_)
        nextZoom = ZOOM_FACTORS[i];
    }
    this.setZoom(nextZoom);
    this.updateViewport_();
  },

  /**
   * Zoom in to the next predefined zoom level.
   */
  zoomIn: function() {
    var nextZoom = ZOOM_FACTORS[ZOOM_FACTORS.length - 1];
    for (var i = ZOOM_FACTORS.length - 1; i >= 0; i--) {
      if (ZOOM_FACTORS[i] > this.zoom_)
        nextZoom = ZOOM_FACTORS[i];
    }
    this.setZoom(nextZoom);
    this.updateViewport_();
  },

  /**
   * Go to the given page index.
   * @param {number} page the index of the page to go to
   */
  goToPage: function(page) {
    if (page < 0)
      page = 0;
    var dimensions = this.pageDimensions_[page];
    this.window_.scrollTo(dimensions.x * this.zoom_, dimensions.y * this.zoom_);
  },

  /**
   * Set the dimensions of the document.
   * @param {Object} documentDimensions the dimensions of the document
   */
  setDocumentDimensions: function(documentDimensions) {
    this.documentDimensions_ = documentDimensions;
    this.pageDimensions_ = this.documentDimensions_.pageDimensions;
    this.contentSizeChanged_();
    this.updateViewport_();
  }
};
