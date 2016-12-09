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
   * @param {number=} opt_snapDistance Provide to override the snap distance.
   *     0 means snap at any distance.
   * @return {number} The moved point. Returns the point itself if it doesn't
   *     need to snap to the edge.
   * @private
   */
  function snapToEdge_(point, width, basePoint, baseWidth, opt_snapDistance) {
    // If the edge of the region is smaller than this, it will snap to the
    // base's edge.
    /** @const */ var SNAP_DISTANCE_PX = 16;
    var snapDist;
    if (opt_snapDistance !== undefined)
      snapDist = opt_snapDistance;
    else
      snapDist = SNAP_DISTANCE_PX;

    var startDiff = Math.abs(point - basePoint);
    var endDiff = Math.abs(point + width - (basePoint + baseWidth));
    // Prefer the closer one if both edges are close enough.
    if ((!snapDist || startDiff < snapDist) && startDiff < endDiff)
      return basePoint;
    else if (!snapDist || endDiff < snapDist)
      return basePoint + baseWidth - width;

    return point;
  }

  /**
   * @constructor
   * @param {string} id
   * @param {string} name
   * @param {!options.DisplayBounds} bounds
   * @param {!options.DisplayLayoutType|undefined} layoutType
   * @param {number|undefined} offset
   * @param {string|undefined} parentId
   * @return {!options.DisplayLayout}
   */
  function DisplayLayout(id, name, bounds, layoutType, offset, parentId) {
    this.bounds = bounds;
    this.id = id;
    if (layoutType != undefined)
      this.layoutType = layoutType;
    this.name = name;
    if (offset != undefined)
      this.offset = offset;
    this.originalDivOffsets = {x: 0, y: 0};
    if (parentId != undefined)
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
     * Calculates the offset relative to |parent|.
     * @param {number} scale
     * @param {?options.DisplayLayout} parent
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
     * Calculates the bounds relative to |parentBounds|.
     * @param {!options.DisplayBounds} parentBounds
     * @return {!options.DisplayBounds}
     */
    calculateBounds: function(parentBounds) {
      var left = 0, top = 0;
      switch (this.layoutType) {
        case options.DisplayLayoutType.TOP:
          left = parentBounds.left + this.offset;
          top = parentBounds.top - this.bounds.height;
          break;
        case options.DisplayLayoutType.RIGHT:
          left = parentBounds.left + parentBounds.width;
          top = parentBounds.top + this.offset;
          break;
        case options.DisplayLayoutType.BOTTOM:
          left = parentBounds.left + this.offset;
          top = parentBounds.top + parentBounds.height;
          break;
        case options.DisplayLayoutType.LEFT:
          left = parentBounds.left - this.bounds.width;
          top = parentBounds.top + this.offset;
          break;
      }
      return {
        left: left,
        top: top,
        width: this.bounds.width,
        height: this.bounds.height
      };
    },

    /**
     * Return the position closest to |newPosition| along the edge of
     * |parentDiv| specified by |layoutType|.
     * @param {options.DisplayPosition} newPosition
     * @param {?HTMLElement} parentDiv
     * @param {!options.DisplayLayoutType} layoutType
     * @return {!options.DisplayPosition}
     */
    getSnapPosition: function(newPosition, parentDiv, layoutType) {
      var x;
      if (layoutType == options.DisplayLayoutType.LEFT) {
        x = parentDiv.offsetLeft - this.div.offsetWidth;
      } else if (layoutType == options.DisplayLayoutType.RIGHT) {
        x = parentDiv.offsetLeft + parentDiv.offsetWidth;
      } else {
        x = this.snapToX(newPosition.x, parentDiv);
      }

      var y;
      if (layoutType == options.DisplayLayoutType.TOP) {
        y = parentDiv.offsetTop - this.div.offsetHeight;
      } else if (layoutType == options.DisplayLayoutType.BOTTOM) {
        y = parentDiv.offsetTop + parentDiv.offsetHeight;
      } else {
        y = this.snapToY(newPosition.y, parentDiv);
      }

      return {x: x, y: y};
    },

    /**
     * Update the div location to the position closest to |newPosition|  along
     * the edge of |parentDiv| specified by |layoutType|.
     * @param {options.DisplayPosition} newPosition
     * @param {?HTMLElement} parentDiv
     * @param {!options.DisplayLayoutType} layoutType
     */
    setDivPosition: function(newPosition, parentDiv, layoutType) {
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
     * Return the position of the corner of the div closest to |pos|.
     * @param {?HTMLElement} parentDiv
     * @param {options.DisplayPosition} pos
     * @return {!options.DisplayPosition}
     * @private
     */
    getCornerPos: function(parentDiv, pos) {
      var x;
      if (pos.x > parentDiv.offsetLeft + parentDiv.offsetWidth / 2)
        x = parentDiv.offsetLeft + parentDiv.offsetWidth;
      else
        x = parentDiv.offsetLeft - this.div.offsetWidth;
      var y;
      if (pos.y > parentDiv.offsetTop + parentDiv.offsetHeight / 2)
        y = parentDiv.offsetTop + parentDiv.offsetHeight;
      else
        y = parentDiv.offsetTop - this.div.offsetHeight;
      return {x: x, y: y};
    },

    /**
     * Ensures that there is a minimum overlap when displays meet at a corner.
     * @param {?HTMLElement} parentDiv
     * @param {options.DisplayLayoutType} layoutType
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
     * Calculates the layoutType for |position| relative to |parentDiv|.
     * @param {?HTMLElement} parentDiv
     * @param {!options.DisplayPosition} position
     * @return {options.DisplayLayoutType}
     */
    getLayoutTypeForPosition: function(parentDiv, position) {
      var div = this.div;

      // Translate position from top-left to center.
      var x = position.x + div.offsetWidth / 2;
      var y = position.y + div.offsetHeight / 2;

      // Determine the distance from the new position to both of the near edges.
      var left = parentDiv.offsetLeft;
      var top = parentDiv.offsetTop;
      var width = parentDiv.offsetWidth;
      var height = parentDiv.offsetHeight;

      // Signed deltas to the center of the div.
      var dx = x - (left + width / 2);
      var dy = y - (top + height / 2);

      // Unsigned distance to each edge.
      var distx = Math.abs(dx) - width / 2;
      var disty = Math.abs(dy) - height / 2;

      if (distx > disty) {
        if (dx < 0)
          return options.DisplayLayoutType.LEFT;
        else
          return options.DisplayLayoutType.RIGHT;
      } else {
        if (dy < 0)
          return options.DisplayLayoutType.TOP;
        else
          return options.DisplayLayoutType.BOTTOM;
      }
    },

    /**
     * Snaps a horizontal value, see SnapToEdge.
     * @param {number} x
     * @param {?HTMLElement} parentDiv
     * @param {number=} opt_snapDistance Provide to override the snap distance.
     *     0 means snap from any distance.
     * @return {number}
     */
    snapToX: function(x, parentDiv, opt_snapDistance) {
      return snapToEdge_(
          x, this.div.offsetWidth, parentDiv.offsetLeft, parentDiv.offsetWidth,
          opt_snapDistance);
    },

    /**
     * Snaps a vertical value, see SnapToEdge.
     * @param {number} y
     * @param {?HTMLElement} parentDiv
     * @param {number=} opt_snapDistance Provide to override the snap distance.
     *     0 means snap from any distance.
     * @return {number}
     */
    snapToY: function(y, parentDiv, opt_snapDistance) {
      return snapToEdge_(
          y, this.div.offsetHeight, parentDiv.offsetTop, parentDiv.offsetHeight,
          opt_snapDistance);
    },

    /**
     * Intersects this.div at |pos| with |otherDiv|. If there is a collision,
     * modifies |deltaPos| to limit movement to a single axis and avoid the
     * collision and returns true.
     * @param {!options.DisplayPosition} pos
     * @param {?HTMLElement} otherDiv
     * @param {!options.DisplayPosition} deltaPos
     * @return {boolean} Whether there was a collision.
     */
    collideWithDivAndModifyDelta: function(pos, otherDiv, deltaPos) {
      var div = this.div;
      var newX = pos.x + deltaPos.x;
      var newY = pos.y + deltaPos.y;

      if ((newX + div.offsetWidth <= otherDiv.offsetLeft) ||
          (newX >= otherDiv.offsetLeft + otherDiv.offsetWidth) ||
          (newY + div.offsetHeight <= otherDiv.offsetTop) ||
          (newY >= otherDiv.offsetTop + otherDiv.offsetHeight)) {
        return false;
      }

      if (Math.abs(deltaPos.x) > Math.abs(deltaPos.y)) {
        if (deltaPos.x > 0) {
          var x = otherDiv.offsetLeft - div.offsetWidth;
          if (x > pos.x)
            deltaPos.x = x - pos.x;
          else
            deltaPos.x = 0;
        } else {
          var x = otherDiv.offsetLeft + otherDiv.offsetWidth;
          if (x < pos.x)
            deltaPos.x = x - pos.x;
          else
            deltaPos.x = 0;
        }
        deltaPos.y = 0;
      } else {
        deltaPos.x = 0;
        if (deltaPos.y > 0) {
          var y = otherDiv.offsetTop - div.offsetHeight;
          if (y > pos.y)
            deltaPos.y = y - pos.y;
          else
            deltaPos.y = 0;
        } else if (deltaPos.y < 0) {
          var y = otherDiv.offsetTop + otherDiv.offsetTop;
          if (y < pos.y)
            deltaPos.y = y - pos.y;
          else
            deltaPos.y = 0;
        }
      }

      return true;
    }
  };

  // Export
  return {DisplayLayout: DisplayLayout};
});
