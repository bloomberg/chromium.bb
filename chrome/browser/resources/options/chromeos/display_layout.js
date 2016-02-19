// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('options');

/**
 * Enumeration of display layout. These values must match the C++ values in
 * ash::DisplayController.
 * @enum {number}
 */
options.DisplayLayoutType = {
  TOP: 0,
  RIGHT: 1,
  BOTTOM: 2,
  LEFT: 3
};

/**
 * @typedef {{
 *   left: number,
 *   top: number,
 *   width: number,
 *   height: number
 * }}
 */
options.DisplayBounds;

/**
 * @typedef {{
 *   x: number,
 *   y: number
 * }}
 */
options.DisplayPosition;

cr.define('options', function() {
  'use strict';

  /**
   * Snaps the region [point, width] to [basePoint, baseWidth] if
   * the [point, width] is close enough to the base's edge.
   * @param {number} point The starting point of the region.
   * @param {number} width The width of the region.
   * @param {number} basePoint The starting point of the base region.
   * @param {number} baseWidth The width of the base region.
   * @return {number} The moved point. Returns the point itself if it doesn't
   *     need to snap to the edge.
   * @private
   */
  function snapToEdge(point, width, basePoint, baseWidth) {
    // If the edge of the region is smaller than this, it will snap to the
    // base's edge.
    /** @const */ var SNAP_DISTANCE_PX = 16;

    var startDiff = Math.abs(point - basePoint);
    var endDiff = Math.abs(point + width - (basePoint + baseWidth));
    // Prefer the closer one if both edges are close enough.
    if (startDiff < SNAP_DISTANCE_PX && startDiff < endDiff)
      return basePoint;
    else if (endDiff < SNAP_DISTANCE_PX)
      return basePoint + baseWidth - width;

    return point;
  }

  /**
   * @constructor
   * @param {string} id
   * @param {string} name
   * @param {!options.DisplayBounds} bounds
   * @param {!options.DisplayLayoutType} layoutType
   * @param {string} parentId
   * @return {!options.DisplayLayout}
   */
  function DisplayLayout(id, name, bounds, layoutType, parentId) {
    this.bounds = bounds;
    this.id = id;
    this.layoutType = layoutType;
    this.name = name;
    this.originalDivOffsets = {x: 0, y: 0};
    this.parentId = parentId;
  }

  // Class describing a display layout.
  DisplayLayout.prototype = {
    /** @type {?options.DisplayBounds} */
    bounds: null,

    /** @type {?HTMLElement} */
    div: null,

    /** @type {string} */
    id: '',

    /** @type {options.DisplayLayoutType} */
    layoutType: options.DisplayLayoutType.TOP,

    /** @type {string} */
    name: '',

    /** @type {number} */
    offset: 0,

    /** @type {?options.DisplayPosition} */
    originalDivOffsets: null,

    /** @type {string} */
    parentId: '',

    /**
     * Calculates the div layout for displayLayout.
     * @param {?options.DisplayPosition} offset
     * @param {number} scale
     * @param {?options.DisplayLayout} parentLayout
     */
    layoutDivFromBounds: function(offset, scale, parentLayout) {
      assert(offset);
      var div = this.div;
      var bounds = this.bounds;

      div.style.width = Math.floor(bounds.width * scale) + 'px';
      div.style.height = Math.floor(bounds.height * scale) + 'px';

      if (!parentLayout) {
        div.style.left = Math.trunc(bounds.left * scale) + offset.x + 'px';
        div.style.top = Math.trunc(bounds.top * scale) + offset.y + 'px';
        return;
      }

      var parentDiv = parentLayout.div;
      switch (this.layoutType) {
        case options.DisplayLayoutType.TOP:
          div.style.left = Math.trunc(bounds.left * scale) + offset.x + 'px';
          div.style.top = parentDiv.offsetTop - div.offsetHeight + 'px';
          break;
        case options.DisplayLayoutType.RIGHT:
          div.style.left = parentDiv.offsetLeft + parentDiv.offsetWidth + 'px';
          div.style.top = Math.trunc(bounds.top * scale) + offset.y + 'px';
          break;
        case options.DisplayLayoutType.BOTTOM:
          div.style.left = Math.trunc(bounds.left * scale) + offset.x + 'px';
          div.style.top = parentDiv.offsetTop + parentDiv.offsetHeight + 'px';
          break;
        case options.DisplayLayoutType.LEFT:
          div.style.left = parentDiv.offsetLeft - div.offsetWidth + 'px';
          div.style.top = Math.trunc(bounds.top * scale) + offset.y + 'px';
          break;
      }
    },

    /**
     * Calculates the offset for displayLayout relative to its parent.
     * @param {number} scale
     * @param {options.DisplayLayout} parent
     */
    calculateOffset: function(scale, parent) {
      // Offset is calculated from top or left edge.
      if (!parent) {
        this.offset = 0;
        return;
      }
      assert(this.parentId == parent.id);
      var offset;
      if (this.layoutType == options.DisplayLayoutType.LEFT ||
          this.layoutType == options.DisplayLayoutType.RIGHT) {
        offset = this.div.offsetTop - parent.div.offsetTop;
      } else {
        offset = this.div.offsetLeft - parent.div.offsetLeft;
      }
      this.offset = Math.trunc(offset / scale);
    },

    /**
     * Update the div location to the position closest to |newPosition|  along
     * the edge of |parentDiv| specified by |layoutType|.
     * @param {options.DisplayPosition} newPosition
     * @param {?HTMLElement} parentDiv
     * @param {!options.DisplayLayoutType} layoutType
     * @private
     */
    setDivPosition(newPosition, parentDiv, layoutType) {
      var div = this.div;
      switch (layoutType) {
        case options.DisplayLayoutType.RIGHT:
          div.style.left = parentDiv.offsetLeft + parentDiv.offsetWidth + 'px';
          div.style.top = newPosition.y + 'px';
          break;
        case options.DisplayLayoutType.LEFT:
          div.style.left = parentDiv.offsetLeft - div.offsetWidth + 'px';
          div.style.top = newPosition.y + 'px';
          break;
        case options.DisplayLayoutType.TOP:
          div.style.top = parentDiv.offsetTop - div.offsetHeight + 'px';
          div.style.left = newPosition.x + 'px';
          break;
        case options.DisplayLayoutType.BOTTOM:
          div.style.top = parentDiv.offsetTop + parentDiv.offsetHeight + 'px';
          div.style.left = newPosition.x + 'px';
          break;
      }
    },

    /**
     * Ensures that there is a minimum overlap when displays meet at a corner.
     * @param {?HTMLElement} parentDiv
     * @param {options.DisplayLayoutType} layoutType
     * @private
     */
    adjustCorners: function(parentDiv, layoutType) {
      // The number of pixels to share the edges between displays.
      /** @const */ var MIN_OFFSET_OVERLAP = 5;

      var div = this.div;
      if (layoutType == options.DisplayLayoutType.LEFT ||
          layoutType == options.DisplayLayoutType.RIGHT) {
        var top = Math.max(
            div.offsetTop,
            parentDiv.offsetTop - div.offsetHeight + MIN_OFFSET_OVERLAP);
        top = Math.min(
            top,
            parentDiv.offsetTop + parentDiv.offsetHeight - MIN_OFFSET_OVERLAP);
        div.style.top = top + 'px';
      } else {
        var left = Math.max(
            div.offsetLeft,
            parentDiv.offsetLeft - div.offsetWidth + MIN_OFFSET_OVERLAP);
        left = Math.min(
            left,
            parentDiv.offsetLeft + parentDiv.offsetWidth - MIN_OFFSET_OVERLAP);
        div.style.left = left + 'px';
      }
    },

    /**
     * Snaps a horizontal value, see SnapToEdge.
     * @param {number} x
     * @param {?HTMLElement} parentDiv
     * @return {number}
     * @private
     */
    snapToX: function(x, parentDiv) {
      return snapToEdge(
          x, this.div.offsetWidth, parentDiv.offsetLeft, parentDiv.offsetWidth);
    },

    /**
     * Snaps a vertical value, see SnapToEdge.
     * @param {number} y
     * @param {?HTMLElement} parentDiv
     * @return {number}
     * @private
     */
    snapToY: function(y, parentDiv) {
      return snapToEdge(
          y, this.div.offsetHeight, parentDiv.offsetTop,
          parentDiv.offsetHeight);
    },
  };

  // Export
  return {DisplayLayout: DisplayLayout};
});
