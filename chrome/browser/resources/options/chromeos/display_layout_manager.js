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

/**
 * @typedef {{
 *   bounds: !options.DisplayBounds,
 *   div: ?HTMLElement,
 *   id: string,
 *   layoutType: options.DisplayLayoutType,
 *   name: string,
 *   offset: number,
 *   originalPosition: !options.DisplayPosition,
 *   parentId: string
 * }}
 */
options.DisplayLayout;

cr.define('options', function() {
  'use strict';

  /**
   * Gets the layout type of |point| relative to |rect|.
   * @param {!options.DisplayBounds} rect The base rectangle.
   * @param {!options.DisplayPosition} point The point to check the position.
   * @return {options.DisplayLayoutType}
   */
  function getPositionToRectangle(rect, point) {
    // Separates the area into four (LEFT/RIGHT/TOP/BOTTOM) by the diagonals of
    // the rect, and decides which area the display should reside.
    var diagonalSlope = rect.height / rect.width;
    var topDownIntercept = rect.top - rect.left * diagonalSlope;
    var bottomUpIntercept = rect.top + rect.height + rect.left * diagonalSlope;

    if (point.y > topDownIntercept + point.x * diagonalSlope) {
      if (point.y > bottomUpIntercept - point.x * diagonalSlope)
        return options.DisplayLayoutType.BOTTOM;
      else
        return options.DisplayLayoutType.LEFT;
    } else {
      if (point.y > bottomUpIntercept - point.x * diagonalSlope)
        return options.DisplayLayoutType.RIGHT;
      else
        return options.DisplayLayoutType.TOP;
    }
  }

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
   */
  function DisplayLayoutManager() {
    this.displayLayoutMap_ = {};
    this.displayAreaOffset_ = {x: 0, y: 0};
  }

  // Helper class for display layout management. Implements logic for laying
  // out two displays.
  DisplayLayoutManager.prototype = {
    /**
     * An object containing DisplayLayout objects for each entry in
     * |displays_|.
     * @type {?Object<!options.DisplayLayout>}
     * @private
     */
    displayLayoutMap_: null,

    /**
     * The scale factor of the actual display size to the drawn display
     * rectangle size. Set in calculateDisplayArea_.
     * @type {number}
     * @private
     */
    visualScale_: 1,

    /**
     * The offset to the center of the display area div.
     * @type {?options.DisplayPosition}
     * @private
     */
    displayAreaOffset_: null,

    /**
     * Adds a display to the layout map.
     * @param {options.DisplayLayout} displayLayout
     */
    addDisplayLayout: function(displayLayout) {
      this.displayLayoutMap_[displayLayout.id] = displayLayout;
    },

    /**
     * Returns the display layout for |id|.
     * @param {string} id
     * @return {options.DisplayLayout}
     */
    getDisplayLayout: function(id) { return this.displayLayoutMap_[id]; },

    /**
     * Returns the number of display layout entries.
     * @return {number}
     */
    getDisplayLayoutCount: function() {
      return Object.keys(/** @type {!Object} */ (this.displayLayoutMap_))
          .length;
    },

    /**
     * Returns the display layout corresponding to |div|.
     * @param {!HTMLElement} div
     * @return {?options.DisplayLayout}
     */
    getFocusedLayoutForDiv: function(div) {
      for (var id in this.displayLayoutMap_) {
        var layout = this.displayLayoutMap_[id];
        if (layout.div == div ||
            (div.offsetParent && layout.div == div.offsetParent)) {
          return layout;
        }
      }
      return null;
    },

    /**
     * Sets the display area div size and creates a div for each entry in
     * displayLayoutMap_.
     * @param {!Element} displayAreaDiv The display area div element.
     * @param {number} minVisualScale The minimum visualScale value.
     * @return {number} The calculated visual scale.
     */
    createDisplayArea: function(displayAreaDiv, minVisualScale) {
      this.calculateDisplayArea_(displayAreaDiv, minVisualScale);
      for (var id in this.displayLayoutMap_) {
        var layout = this.displayLayoutMap_[id];
        if (layout.div) {
          // Parent divs must be created before children, so the div for this
          // entry may have already been created.
          continue;
        }
        this.createDisplayLayoutDiv_(id, displayAreaDiv);
      }
      return this.visualScale_;
    },

    /**
     * Sets the display layout div corresponding to |id| to focused and
     * sets all other display layouts to unfocused.
     * @param {string} focusedId
     */
    setFocusedId: function(focusedId) {
      for (var id in this.displayLayoutMap_) {
        var layout = this.displayLayoutMap_[id];
        layout.div.classList.toggle('displays-focused', layout.id == focusedId);
      }
    },

    /**
     * Sets the mouse event callbacks for each div.
     * @param {function (Event)} onMouseDown
     * @param {function (Event)} onTouchStart
     */
    setDivCallbacks: function(onMouseDown, onTouchStart) {
      for (var id in this.displayLayoutMap_) {
        var layout = this.displayLayoutMap_[id];
        layout.div.onmousedown = onMouseDown;
        layout.div.ontouchstart = onTouchStart;
      }
    },

    /**
     * Update the location of display |id| to |newPosition|.
     * @param {string} id
     * @param {options.DisplayPosition} newPosition
     * @private
     */
    updatePosition: function(id, newPosition) {
      var displayLayout = this.displayLayoutMap_[id];
      var div = displayLayout.div;
      var baseLayout = this.getBaseLayout_(displayLayout);
      var baseDiv = baseLayout.div;

      newPosition.x = snapToEdge(
          newPosition.x, div.offsetWidth, baseDiv.offsetLeft,
          baseDiv.offsetWidth);
      newPosition.y = snapToEdge(
          newPosition.y, div.offsetHeight, baseDiv.offsetTop,
          baseDiv.offsetHeight);

      /** @type {!options.DisplayPosition} */ var newCenter = {
        x: newPosition.x + div.offsetWidth / 2,
        y: newPosition.y + div.offsetHeight / 2
      };

      /** @type {!options.DisplayBounds} */ var baseBounds = {
        left: baseDiv.offsetLeft,
        top: baseDiv.offsetTop,
        width: baseDiv.offsetWidth,
        height: baseDiv.offsetHeight
      };

      // This implementation considers only the case of two displays, i.e
      // a single parent with one child.
      var isPrimary = displayLayout.parentId == '';

      // layoutType is always stored in the child layout.
      var layoutType =
          isPrimary ? baseLayout.layoutType : displayLayout.layoutType;

      switch (getPositionToRectangle(baseBounds, newCenter)) {
        case options.DisplayLayoutType.RIGHT:
          layoutType = isPrimary ? options.DisplayLayoutType.LEFT :
                                   options.DisplayLayoutType.RIGHT;
          break;
        case options.DisplayLayoutType.LEFT:
          layoutType = isPrimary ? options.DisplayLayoutType.RIGHT :
                                   options.DisplayLayoutType.LEFT;
          break;
        case options.DisplayLayoutType.TOP:
          layoutType = isPrimary ? options.DisplayLayoutType.BOTTOM :
                                   options.DisplayLayoutType.TOP;
          break;
        case options.DisplayLayoutType.BOTTOM:
          layoutType = isPrimary ? options.DisplayLayoutType.TOP :
                                   options.DisplayLayoutType.BOTTOM;
          break;
      }

      if (layoutType == options.DisplayLayoutType.LEFT ||
          layoutType == options.DisplayLayoutType.RIGHT) {
        if (newPosition.y > baseDiv.offsetTop + baseDiv.offsetHeight)
          layoutType = isPrimary ? options.DisplayLayoutType.TOP :
                                   options.DisplayLayoutType.BOTTOM;
        else if (newPosition.y + div.offsetHeight < baseDiv.offsetTop)
          layoutType = isPrimary ? options.DisplayLayoutType.BOTTOM :
                                   options.DisplayLayoutType.TOP;
      } else {
        if (newPosition.x > baseDiv.offsetLeft + baseDiv.offsetWidth)
          layoutType = isPrimary ? options.DisplayLayoutType.LEFT :
                                   options.DisplayLayoutType.RIGHT;
        else if (newPosition.x + div.offsetWidth < baseDiv.offsetLeft)
          layoutType = isPrimary ? options.DisplayLayoutType.RIGHT :
                                   options.DisplayLayoutType.LEFT;
      }

      var layoutToBase;
      if (!isPrimary) {
        displayLayout.layoutType = layoutType;
        layoutToBase = layoutType;
      } else {
        baseLayout.layoutType = layoutType;
        switch (layoutType) {
          case options.DisplayLayoutType.RIGHT:
            layoutToBase = options.DisplayLayoutType.LEFT;
            break;
          case options.DisplayLayoutType.LEFT:
            layoutToBase = options.DisplayLayoutType.RIGHT;
            break;
          case options.DisplayLayoutType.TOP:
            layoutToBase = options.DisplayLayoutType.BOTTOM;
            break;
          case options.DisplayLayoutType.BOTTOM:
            layoutToBase = options.DisplayLayoutType.TOP;
            break;
        }
      }

      switch (layoutToBase) {
        case options.DisplayLayoutType.RIGHT:
          div.style.left = baseDiv.offsetLeft + baseDiv.offsetWidth + 'px';
          div.style.top = newPosition.y + 'px';
          break;
        case options.DisplayLayoutType.LEFT:
          div.style.left = baseDiv.offsetLeft - div.offsetWidth + 'px';
          div.style.top = newPosition.y + 'px';
          break;
        case options.DisplayLayoutType.TOP:
          div.style.top = baseDiv.offsetTop - div.offsetHeight + 'px';
          div.style.left = newPosition.x + 'px';
          break;
        case options.DisplayLayoutType.BOTTOM:
          div.style.top = baseDiv.offsetTop + baseDiv.offsetHeight + 'px';
          div.style.left = newPosition.x + 'px';
          break;
      }
    },

    /**
     * Called from the UI to finalize the location of display |id| after updates
     * are complete (e.g. after a drag was completed).
     * @param {string} id
     * @return {boolean} True if the final position differs from the original.
     */
    finalizePosition: function(id) {
      // Make sure the dragging location is connected.
      var displayLayout = this.displayLayoutMap_[id];
      var div = displayLayout.div;
      var baseLayout = this.getBaseLayout_(displayLayout);
      var baseDiv = baseLayout.div;

      var isPrimary = displayLayout.parentId == '';
      var layoutType =
          isPrimary ? baseLayout.layoutType : displayLayout.layoutType;

      // The number of pixels to share the edges between displays.
      /** @const */ var MIN_OFFSET_OVERLAP = 5;

      if (layoutType == options.DisplayLayoutType.LEFT ||
          layoutType == options.DisplayLayoutType.RIGHT) {
        var top = Math.max(
            div.offsetTop,
            baseDiv.offsetTop - div.offsetHeight + MIN_OFFSET_OVERLAP);
        top = Math.min(
            top, baseDiv.offsetTop + baseDiv.offsetHeight - MIN_OFFSET_OVERLAP);
        div.style.top = top + 'px';
      } else {
        var left = Math.max(
            div.offsetLeft,
            baseDiv.offsetLeft - div.offsetWidth + MIN_OFFSET_OVERLAP);
        left = Math.min(
            left,
            baseDiv.offsetLeft + baseDiv.offsetWidth - MIN_OFFSET_OVERLAP);
        div.style.left = left + 'px';
      }

      // Calculate the offset of the child display.
      this.calculateOffset_(isPrimary ? baseLayout : displayLayout);

      return displayLayout.originalPosition.x != div.offsetLeft ||
          displayLayout.originalPosition.y != div.offsetTop;
    },

    /**
     * Calculates the display area offset and scale.
     * @param {!Element} displayAreaDiv The containing display area div.
     * @param {number} minVisualScale The minimum visualScale value.
     */
    calculateDisplayArea_(displayAreaDiv, minVisualScale) {
      var maxWidth = 0;
      var maxHeight = 0;
      var boundingBox = {left: 0, right: 0, top: 0, bottom: 0};

      for (var id in this.displayLayoutMap_) {
        var layout = this.displayLayoutMap_[id];
        var bounds = layout.bounds;
        boundingBox.left = Math.min(boundingBox.left, bounds.left);
        boundingBox.right =
            Math.max(boundingBox.right, bounds.left + bounds.width);
        boundingBox.top = Math.min(boundingBox.top, bounds.top);
        boundingBox.bottom =
            Math.max(boundingBox.bottom, bounds.top + bounds.height);
        maxWidth = Math.max(maxWidth, bounds.width);
        maxHeight = Math.max(maxHeight, bounds.height);
      }

      // Make the margin around the bounding box.
      var areaWidth = boundingBox.right - boundingBox.left + maxWidth;
      var areaHeight = boundingBox.bottom - boundingBox.top + maxHeight;

      // Calculates the scale by the width since horizontal size is more strict.
      // TODO(mukai): Adds the check of vertical size in case.
      this.visualScale_ =
          Math.min(minVisualScale, displayAreaDiv.offsetWidth / areaWidth);

      // Prepare enough area for displays_view by adding the maximum height.
      displayAreaDiv.style.height =
          Math.ceil(areaHeight * this.visualScale_) + 'px';

      // Centering the bounding box of the display rectangles.
      this.displayAreaOffset_ = {
        x: Math.floor(
            displayAreaDiv.offsetWidth / 2 -
            (boundingBox.right + boundingBox.left) * this.visualScale_ / 2),
        y: Math.floor(
            displayAreaDiv.offsetHeight / 2 -
            (boundingBox.bottom + boundingBox.top) * this.visualScale_ / 2)
      };
    },

    /**
     * Creates a div element and assigns it to |displayLayout|. Returns the
     * created div for additional decoration.
     * @param {string} id
     * @param {!Element} displayAreaDiv The containing display area div.
     */
    createDisplayLayoutDiv_: function(id, displayAreaDiv) {
      var displayLayout = this.displayLayoutMap_[id];
      var parentId = displayLayout.parentId;
      var offset = this.displayAreaOffset_;
      if (parentId) {
        // Ensure the parent div is created first.
        var parentLayout = this.displayLayoutMap_[parentId];
        if (!parentLayout.div)
          this.createDisplayLayoutDiv_(parentId, displayAreaDiv);
      }

      var div = /** @type {!HTMLElement} */ (document.createElement('div'));
      div.className = 'displays-display';

      // div needs to be added to the DOM tree first, otherwise offsetHeight for
      // nameContainer below cannot be computed.
      displayAreaDiv.appendChild(div);

      var nameContainer = document.createElement('div');
      nameContainer.textContent = displayLayout.name;
      div.appendChild(nameContainer);

      var bounds = displayLayout.bounds;
      div.style.width = Math.floor(bounds.width * this.visualScale_) + 'px';
      var newHeight = Math.floor(bounds.height * this.visualScale_);
      div.style.height = newHeight + 'px';
      nameContainer.style.marginTop =
          (newHeight - nameContainer.offsetHeight) / 2 + 'px';

      if (displayLayout.parentId == '') {
        div.style.left =
            Math.floor(bounds.left * this.visualScale_) + offset.x + 'px';
        div.style.top =
            Math.floor(bounds.top * this.visualScale_) + offset.y + 'px';
      } else {
        // Don't trust the child display's x or y, because it may cause a
        // 1px gap due to rounding, which will create a fake update on end
        // dragging. See crbug.com/386401
        var parentDiv = this.displayLayoutMap_[displayLayout.parentId].div;
        switch (displayLayout.layoutType) {
          case options.DisplayLayoutType.TOP:
            div.style.left =
                Math.floor(bounds.left * this.visualScale_) + offset.x + 'px';
            div.style.top = parentDiv.offsetTop - div.offsetHeight + 'px';
            break;
          case options.DisplayLayoutType.RIGHT:
            div.style.left =
                parentDiv.offsetLeft + parentDiv.offsetWidth + 'px';
            div.style.top =
                Math.floor(bounds.top * this.visualScale_) + offset.y + 'px';
            break;
          case options.DisplayLayoutType.BOTTOM:
            div.style.left =
                Math.floor(bounds.left * this.visualScale_) + offset.x + 'px';
            div.style.top = parentDiv.offsetTop + parentDiv.offsetHeight + 'px';
            break;
          case options.DisplayLayoutType.LEFT:
            div.style.left = parentDiv.offsetLeft - div.offsetWidth + 'px';
            div.style.top =
                Math.floor(bounds.top * this.visualScale_) + offset.y + 'px';
            break;
        }
      }

      displayLayout.div = div;
      displayLayout.originalPosition.x = div.offsetLeft;
      displayLayout.originalPosition.y = div.offsetTop;

      this.calculateOffset_(displayLayout);
    },

    /**
     * Calculates the offset for display |id| relative to its parent.
     * @param {options.DisplayLayout} displayLayout
     */
    calculateOffset_: function(displayLayout) {
      // Offset is calculated from top or left edge.
      var parent = this.displayLayoutMap_[displayLayout.parentId];
      if (!parent) {
        displayLayout.offset = 0;
        return;
      }
      var offset;
      if (displayLayout.layoutType == options.DisplayLayoutType.LEFT ||
          displayLayout.layoutType == options.DisplayLayoutType.RIGHT) {
        offset = displayLayout.div.offsetTop - parent.div.offsetTop;
      } else {
        offset = displayLayout.div.offsetLeft - parent.div.offsetLeft;
      }
      displayLayout.offset = Math.floor(offset / this.visualScale_);
    },

    /**
     * Returns the display paired with |displayLayout|, either the parent
     * or a matching child (currently there will only be one).
     * @param {options.DisplayLayout} displayLayout
     * @return {?options.DisplayLayout}
     * @private
     */
    getBaseLayout_(displayLayout) {
      if (displayLayout.parentId != '')
        return this.displayLayoutMap_[displayLayout.parentId];
      for (var childId in this.displayLayoutMap_) {
        var child = this.displayLayoutMap_[childId];
        if (child.parentId == displayLayout.id)
          return child;
      }
      assertNotReached();
      return null;
    }
  };

  // Export
  return {DisplayLayoutManager: DisplayLayoutManager};
});
