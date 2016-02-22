// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('options');

cr.define('options', function() {
  'use strict';

  /**
   * Gets the layout type of |point| relative to |rect|.
   * @param {!options.DisplayBounds} rect The base rectangle.
   * @param {!options.DisplayPosition} point The point to check the position.
   * @return {options.DisplayLayoutType}
   */
  function getLayoutTypeForPosition(rect, point) {
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
   * @param {options.DisplayLayoutType} layoutType
   * @return {!options.DisplayLayoutType}
   */
  function invertLayoutType(layoutType) {
    switch (layoutType) {
      case options.DisplayLayoutType.RIGHT:
        return options.DisplayLayoutType.LEFT;
      case options.DisplayLayoutType.LEFT:
        return options.DisplayLayoutType.RIGHT;
      case options.DisplayLayoutType.TOP:
        return options.DisplayLayoutType.BOTTOM;
      case options.DisplayLayoutType.BOTTOM:
        return options.DisplayLayoutType.TOP;
    }
    assertNotReached();
    return layoutType;
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
     * @param {boolean=} opt_userAction If true, focus was triggered by a
     *     user action. (Used in DisplayLayoutManagerMulti override).
     */
    setFocusedId: function(focusedId, opt_userAction) {
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

      newPosition.x = displayLayout.snapToX(newPosition.x, baseDiv);
      newPosition.y = displayLayout.snapToY(newPosition.y, baseDiv);

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

      var layoutType = getLayoutTypeForPosition(baseBounds, newCenter);
      if (isPrimary)
        layoutType = invertLayoutType(layoutType);

      if (layoutType == options.DisplayLayoutType.LEFT ||
          layoutType == options.DisplayLayoutType.RIGHT) {
        if (newPosition.y > baseDiv.offsetTop + baseDiv.offsetHeight) {
          layoutType = isPrimary ? options.DisplayLayoutType.TOP :
                                   options.DisplayLayoutType.BOTTOM;
        } else if (newPosition.y + div.offsetHeight < baseDiv.offsetTop) {
          layoutType = isPrimary ? options.DisplayLayoutType.BOTTOM :
                                   options.DisplayLayoutType.TOP;
        }
      } else {
        if (newPosition.x > baseDiv.offsetLeft + baseDiv.offsetWidth) {
          layoutType = isPrimary ? options.DisplayLayoutType.LEFT :
                                   options.DisplayLayoutType.RIGHT;
        } else if (newPosition.x + div.offsetWidth < baseDiv.offsetLeft) {
          layoutType = isPrimary ? options.DisplayLayoutType.RIGHT :
                                   options.DisplayLayoutType.LEFT;
        }
      }

      // layout is always relative to primary and stored in the child layout.
      var layoutToBase;
      if (!isPrimary) {
        displayLayout.layoutType = layoutType;
        layoutToBase = layoutType;
      } else {
        baseLayout.layoutType = layoutType;
        layoutToBase = invertLayoutType(layoutType);
      }

      displayLayout.setDivPosition(newPosition, baseDiv, layoutToBase);
    },

    /**
     * Called from the UI to finalize the location of display |id| after updates
     * are complete (e.g. after a drag was completed).
     * @param {string} id
     * @return {boolean} True if the final position differs from the original.
     */
    finalizePosition: function(id) {
      var displayLayout = this.displayLayoutMap_[id];
      var div = displayLayout.div;
      var baseLayout = this.getBaseLayout_(displayLayout);

      var isPrimary = displayLayout.parentId == '';
      var layoutType =
          isPrimary ? baseLayout.layoutType : displayLayout.layoutType;

      // Make sure the dragging location is connected.
      displayLayout.adjustCorners(baseLayout.div, layoutType);

      // Calculate the offset of the child display.
      if (isPrimary) {
        baseLayout.calculateOffset(this.visualScale_, displayLayout);
      } else {
        var parent = this.displayLayoutMap_[displayLayout.parentId];
        displayLayout.calculateOffset(this.visualScale_, parent);
      }

      return displayLayout.originalDivOffsets.x != div.offsetLeft ||
          displayLayout.originalDivOffsets.y != div.offsetTop;
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
      var parentLayout = null;
      if (parentId) {
        // Ensure the parent div is created first.
        parentLayout = this.displayLayoutMap_[parentId];
        if (!parentLayout.div)
          this.createDisplayLayoutDiv_(parentId, displayAreaDiv);
      }

      var div = /** @type {!HTMLElement} */ (document.createElement('div'));
      div.className = 'displays-display';
      displayLayout.div = div;

      // div needs to be added to the DOM tree first, otherwise offsetHeight for
      // nameContainer below cannot be computed.
      displayAreaDiv.appendChild(div);

      var nameContainer = document.createElement('div');
      nameContainer.textContent = displayLayout.name;
      div.appendChild(nameContainer);

      var newHeight =
          Math.floor(displayLayout.bounds.height * this.visualScale_);
      nameContainer.style.marginTop =
          (newHeight - nameContainer.offsetHeight) / 2 + 'px';

      displayLayout.layoutDivFromBounds(
          this.displayAreaOffset_, this.visualScale_, parentLayout);

      displayLayout.originalDivOffsets.x = div.offsetLeft;
      displayLayout.originalDivOffsets.y = div.offsetTop;

      displayLayout.calculateOffset(this.visualScale_, parentLayout);
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
