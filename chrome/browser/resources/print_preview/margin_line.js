// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'strict';

  function MarginLine(groupName) {
    var line = document.createElement('div');
    line.__proto__ = MarginLine.prototype;
    line.className = MarginLine.CSS_CLASS_DRAGGABLE_AREA;
    // @type {string} Specifies which margin this line refers to.
    line.marginGroup = groupName;
    // @type {print_preview.Rect} A rectangle describing the values of all
    //     margins.
    line.rectangle = null;
    // @type {HTMLDivElement} The dotted line representing the margin.
    line.visibleLine = document.createElement('div');
    line.visibleLine.className = MarginLine.CSS_CLASS_MARGIN_LINE;

    line.appendChild(line.visibleLine);
    return line;
  }

  MarginLine.CSS_CLASS_MARGIN_LINE = 'margin-line';
  MarginLine.CSS_CLASS_DRAGGABLE_AREA = 'draggable-area';
  // Width of the clickable region around each margin line in screen pixels.
  MarginLine.CLICKABLE_REGION = 20;

  MarginLine.prototype = {
    __proto__: HTMLDivElement.prototype,

    update: function(marginsRectangle) {
      this.rectangle = this.getCoordinates_(marginsRectangle);
    },

    /**
     * Draws |this| on screen. Essentially two divs are being drawn, the drag
     * control area (invisible) and the dotted margin line (visible).
     */
    draw: function() {
      this.drawDraggableArea_();
      this.drawDottedLine_();
    },

    /**
     * Draws the dotted line representing the margin.
     * @private
     */
     drawDottedLine_ : function() {
      var rectangle = this.getVisibleLineCoordinates_();
      this.visibleLine.style.left = 100 * rectangle.x + '%';
      this.visibleLine.style.top = 100 * rectangle.y + '%';
      this.visibleLine.style.width = 100 * rectangle.width + '%';
      this.visibleLine.style.height = 100 * rectangle.height + '%';
    },

    /**
     * Draws the area the draggable area (not visible).
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
      var pageLocation = previewArea.getPageLocationNormalized();
      var totalWidth = previewArea.pdfPlugin_.offsetWidth;
      var totalHeight = previewArea.pdfPlugin_.offsetHeight;
      var offsetY = (MarginLine.CLICKABLE_REGION / 2) / totalHeight;
      var offsetX = (MarginLine.CLICKABLE_REGION / 2) / totalWidth;

      if (this.isTop_()) {
        var lineCoordinates = new print_preview.Rect(
            pageLocation.x,
            marginsRectangle.y - offsetY,
            pageLocation.width,
            MarginLine.CLICKABLE_REGION / totalHeight);
      } else if (this.isBottom_()) {
        var lineCoordinates = new print_preview.Rect(
            pageLocation.x,
            marginsRectangle.bottom - offsetY,
            pageLocation.width,
            MarginLine.CLICKABLE_REGION / totalHeight);
      } else if (this.isRight_()) {
        var lineCoordinates = new print_preview.Rect(
            marginsRectangle.right - offsetX,
            pageLocation.y,
            MarginLine.CLICKABLE_REGION / totalWidth,
            pageLocation.height);
      } else if (this.isLeft_()) {
        var lineCoordinates = new print_preview.Rect(
            marginsRectangle.x - offsetX,
            pageLocation.y,
            MarginLine.CLICKABLE_REGION / totalWidth,
            pageLocation.height);
      }
      return lineCoordinates;
    },

    /**
     * Calculates the coordinates in percentages and size of the visible margin
     * line, with respect to |this| div element.
     * @return {print_preview.Rect} A rectangle describing the position of the
     *     visible line in percentages.
     * @private
     */
    getVisibleLineCoordinates_: function() {
      if (this.isHorizontal_())
        var innerMarginsRect = new print_preview.Rect(0, 0.5, 1, 0);
      else if (this.isVertical_())
        var innerMarginsRect = new print_preview.Rect(0.5, 0, 0, 1);
      return innerMarginsRect;
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
     * @return {boolean} True if |this| is a horizontal line.
     * @private
     */
    isHorizontal_: function() {
      return this.isTop_() || this.isBottom_() ;
    },

    /**
     * @return {boolean} True if |this| is a vertical line.
     * @private
     */
    isVertical_: function() {
      return this.isLeft_() || this.isRight_();
    },

  };

  return {
    MarginLine: MarginLine
  };
});
