// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'strict';

  /**
   * @constructor
   * This class represents a margin line and a textbox corresponding to that
   * margin.
   */
  function MarginsUIPair(groupName) {
    var marginsUIPair = document.createElement('div');
    marginsUIPair.__proto__ = MarginsUIPair.prototype;
    marginsUIPair.className = MarginsUIPair.CSS_CLASS;
    // @type {string} Specifies which margin this line refers to.
    marginsUIPair.marginGroup = groupName;
    marginsUIPair.classList.add(groupName);

    // @type {print_preview.Rect} A rectangle describing the dimensions of
    //     the draggable area.
    marginsUIPair.rectangle = null;
    // @type {print_preview.Rect} A rectangle describing the four margins.
    marginsUIPair.marginsRectangle = null;
    // @type {HTMLDivElement} The line representing the margin.
    marginsUIPair.line_ = document.createElement('div');
    marginsUIPair.line_.className = 'margin-line';
    // @type {print_preview.MarginTextbox} The textbox corresponding to this
    //     margin.
    marginsUIPair.box_ = new print_preview.MarginTextbox(groupName);
    // @type {{x: number, y: number}} The point where mousedown occured within
    //     the draggable area with respect the top-left corner of |this|.
    marginsUIPair.mousePointerOffset = null;
    // @type {{x: number, y:number}} The position of the origin before any
    //     dragging in progress occured.
    marginsUIPair.originBeforeDragging = null;

    marginsUIPair.appendChild(marginsUIPair.line_);
    marginsUIPair.appendChild(marginsUIPair.box_);

    marginsUIPair.addEventListeners_();
    return marginsUIPair;
  }

  MarginsUIPair.CSS_CLASS = 'margins-ui-pair';
  // Width of the clickable region around each margin line in screen pixels.
  MarginsUIPair.CLICKABLE_REGION = 20;

  MarginsUIPair.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Updates the state.
     */
    update: function(marginsRectangle, value, valueLimit, keepDisplayedValue,
        valueLimitInPercent) {
      this.marginsRectangle = marginsRectangle;
      this.valueLimitInPercent = valueLimitInPercent;
      this.rectangle = this.getCoordinates_(this.marginsRectangle);
      this.box_.update(value, valueLimit, keepDisplayedValue);
    },

    /**
     * Draws |this| based on the state.
     */
    draw: function() {
      this.drawDraggableArea_();
      this.box_.draw();
    },

    /**
     * Draws the area that can be clicked in order to start dragging.
     * @private
     */
    drawDraggableArea_: function() {
      var width = previewArea.pdfPlugin_.offsetWidth;
      var height = previewArea.pdfPlugin_.offsetHeight;

      this.style.left = Math.round(this.rectangle.x * width) + 'px';
      this.style.top = Math.round(this.rectangle.y * height) + 'px';
      this.style.width = Math.round(this.rectangle.width * width) + 'px';
      this.style.height = Math.round(this.rectangle.height * height) + 'px';
    },

    /**
     * Calculates the coordinates and size of |this|.
     * @param {print_preview.Rect} marginsRectangle A rectangle describing the
     *     selected margins values in percentages.
     * @private
     */
    getCoordinates_: function(marginsRectangle) {
      var pageLocation = previewArea.pageLocationNormalized;
      var totalWidth = previewArea.pdfPlugin_.offsetWidth;
      var totalHeight = previewArea.pdfPlugin_.offsetHeight;
      var offsetY = (MarginsUIPair.CLICKABLE_REGION / 2) / totalHeight;
      var offsetX = (MarginsUIPair.CLICKABLE_REGION / 2) / totalWidth;

      if (this.isTop_()) {
        var lineCoordinates = new print_preview.Rect(
            pageLocation.x,
            marginsRectangle.y - offsetY,
            pageLocation.width,
            MarginsUIPair.CLICKABLE_REGION / totalHeight);
      } else if (this.isBottom_()) {
        var lineCoordinates = new print_preview.Rect(
            pageLocation.x,
            marginsRectangle.bottom - offsetY,
            pageLocation.width,
            MarginsUIPair.CLICKABLE_REGION / totalHeight);
      } else if (this.isRight_()) {
        var lineCoordinates = new print_preview.Rect(
            marginsRectangle.right - offsetX,
            pageLocation.y,
            MarginsUIPair.CLICKABLE_REGION / totalWidth,
            pageLocation.height);
      } else if (this.isLeft_()) {
        var lineCoordinates = new print_preview.Rect(
            marginsRectangle.x - offsetX,
            pageLocation.y,
            MarginsUIPair.CLICKABLE_REGION / totalWidth,
            pageLocation.height);
      }
      return lineCoordinates;
    },

    /**
     * @return {boolean} True if |this| refers to the top margin.
     * @private
     */
    isTop_: function() {
      return this.marginGroup == print_preview.MarginSettings.TOP_GROUP;
    },

    /**
     * @return {boolean} True if |this| refers to the bottom margin.
     * @private
     */
    isBottom_: function() {
      return this.marginGroup == print_preview.MarginSettings.BOTTOM_GROUP;
    },

    /**
     * @return {boolean} True if |this| refers to the left margin.
     * @private
     */
    isLeft_: function() {
      return this.marginGroup == print_preview.MarginSettings.LEFT_GROUP;
    },

    /**
     * @return {boolean} True if |this| refers to the bottom margin.
     * @private
     */
    isRight_: function() {
      return this.marginGroup == print_preview.MarginSettings.RIGHT_GROUP;
    },

    /**
     * Adds event listeners for events related to dragging.
     */
    addEventListeners_: function() {
      this.onmousedown = this.onMouseDown_.bind(this);
    },

    /**
     * Executes whenever a mousedown event occurs on |this| or its child nodes.
     * @param {MouseEvent} e The event that occured.
     */
    onMouseDown_: function(e) {
      if (e.button != 0)
        return;
      if (e.target != this)
        return;
      var point = print_preview.MarginsUI.convert({x: e.x, y: e.y});
      this.originBeforeDragging = { x: this.offsetLeft, y: this.offsetTop };
      this.mousePointerOffset =
          { x: point.x - this.offsetLeft, y: point.y - this.offsetTop };
      cr.dispatchSimpleEvent(this, customEvents.MARGIN_LINE_MOUSE_DOWN);
    },

    /**
     * Executes whenever a mouseup event occurs while |this| is dragged.
     */
    onMouseUp: function() {
      this.box_.onTextValueMayHaveChanged();
    },

    /**
     * Moves |this| including all its children to |point|.
     * @param {{x: number, y: number}} point The point where |this| should be
     *     moved.
     */
    moveTo: function(point) {
      if (this.isTop_() || this.isBottom_())
        this.style.top = point.y - this.mousePointerOffset.y + 'px';
      else
        this.style.left = point.x - this.mousePointerOffset.x + 'px';
    },

    /**
     * @param {{x: number, y: number}} rhs The point to compare with.
     * @return {number} The horizontal or vertical displacement in pixels
     *     between |this.originBeforeDragging| and |rhs|. Note: Bottom margin
     *     grows upwards, right margin grows when going to the left.
     */
    getDragDisplacementFrom: function(rhs) {
      var dragDisplacement = 0;
      if (this.isTop_() || this.isBottom_()) {
        dragDisplacement = (rhs.y - this.originBeforeDragging.y -
            this.mousePointerOffset.y) / previewArea.height;
      } else {
        dragDisplacement = (rhs.x - this.originBeforeDragging.x -
            this.mousePointerOffset.x) / previewArea.width;
      }

      if (this.isBottom_() || this.isRight_())
        dragDisplacement *= -1;
      return dragDisplacement;
    },

    /**
     * Updates |this| while dragging is in progress. Takes care of rejecting
     * invalid margin values.
     * @param {number} dragDeltaInPoints The difference in points between the
     *     currently applied margin and the margin indicated by
     *     |destinationPoint|.
     * @param {{x: number, y: number}} destinationPoint The point where the
     *     margin line should be drawn if |dragDeltaInPoints| is applied.
     */
    updateWhileDragging: function(dragDeltaInPoints, destinationPoint) {
      this.box_.updateWhileDragging(dragDeltaInPoints);
      var validity = this.box_.validateDelta(dragDeltaInPoints);
      if (validity == print_preview.marginValidationStates.WITHIN_RANGE)
        this.moveTo(destinationPoint);
      else if (validity == print_preview.marginValidationStates.TOO_SMALL)
        this.snapToMinValue_();
      else if (validity == print_preview.marginValidationStates.TOO_BIG)
        this.snapToMaxValue_();
    },

    /**
     * Snaps |this| to the minimum allowed margin value (0). Happens whenever
     * the user drags the margin line to a smaller value than the minimum
     * allowed.
     * @private
     */
    snapToMinValue_: function() {
      var pageInformation = previewArea.pageLocationNormalized;
      var newMarginsRectangle = this.marginsRectangle.clone();
      if (this.isTop_()) {
        newMarginsRectangle.y = pageInformation.y;
      } else if (this.isLeft_()) {
        newMarginsRectangle.x = pageInformation.x;
      } else if (this.isRight_()) {
        newMarginsRectangle.x = pageInformation.x;
        newMarginsRectangle.width = pageInformation.width;
      } else if (this.isBottom_()) {
        newMarginsRectangle.y = pageInformation.y;
        newMarginsRectangle.height = pageInformation.height;
      }

      this.rectangle = this.getCoordinates_(newMarginsRectangle);
      this.drawDraggableArea_();
    },

    /**
     * Snaps |this| to the maximum allowed margin value. Happens whenever
     * the user drags the margin line to a larger value than the maximum
     * allowed.
     * @private
     */
    snapToMaxValue_: function() {
      var newMarginsRectangle = this.marginsRectangle.clone();

      if (this.isTop_()) {
        newMarginsRectangle.y = this.valueLimitInPercent;
      } else if (this.isLeft_()) {
        newMarginsRectangle.x = this.valueLimitInPercent;
      } else if (this.isRight_()) {
        newMarginsRectangle.x = 0;
        newMarginsRectangle.width = this.valueLimitInPercent;
      } else if (this.isBottom_()) {
        newMarginsRectangle.y = 0;
        newMarginsRectangle.height = this.valueLimitInPercent;
      }

      this.rectangle = this.getCoordinates_(newMarginsRectangle);
      this.drawDraggableArea_();
    },
  };

  return {
    MarginsUIPair: MarginsUIPair
  };
});
