// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'strict';

  function MarginLine(groupName) {
    var line = document.createElement('div');
    line.__proto__ = MarginLine.prototype;
    line.className = MarginLine.CSS_CLASS_MARGIN_LINE;

    // @type {string} Specifies which margin this line refers to.
    line.marginGroup = groupName;

    return line;
  }

  MarginLine.CSS_CLASS_MARGIN_LINE = 'margin-line';

  MarginLine.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Draws a dotted line representing the margin.
     */
    draw: function() {
      var rectangle = this.getCoordinates_();
      this.style.left = 100 * rectangle.x + '%';
      this.style.top = 100 * rectangle.y + '%';
      this.style.width = 100 * rectangle.width + '%';
      this.style.height = 100 * rectangle.height + '%';
    },

    /**
     * Calculates the coordinates and size of the margin line in percentages,
     * with respect to parent element.
     * @return {print_preview.Rect} A rectangle describing the position of the
     *     visible line in percentages.
     * @private
     */
    getCoordinates_: function() {
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
