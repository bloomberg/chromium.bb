// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'strict';

  function MarginTextbox(groupName) {
    var box = document.createElement('input');
    box.__proto__ = MarginTextbox.prototype;
    box.setAttribute('type', 'text');
    box.className = MarginTextbox.CSS_CLASS_MARGIN_TEXTBOX;
    box.value = '0';

    // @type {string} Specifies which margin this line refers to.
    box.marginGroup = groupName;
    // @type {boolean} True if the displayed value is valid.
    box.isValid = true;
    // @type {number} Timer used to detect when the user stops typing.
    box.timerId_ = null;
    // @type {number} The last valid value in points.
    box.lastValidValueInPoints = 0;
    // @type {print_preview.Rect} A rectangle describing the four margins.
    box.marginsRectangle_ = null;
    // @type {number} The upper allowed limit for the corresponding margin.
    box.valueLimit = null;

    box.addEventListeners_();
    return box;
  }

  MarginTextbox.CSS_CLASS_MARGIN_TEXTBOX = 'margin-box';
  MarginTextbox.MARGIN_BOX_HEIGHT = 15;
  MarginTextbox.MARGIN_BOX_VERTICAL_PADDING = 5;
  MarginTextbox.MARGIN_BOX_WIDTH = 40;
  MarginTextbox.MARGIN_BOX_HORIZONTAL_PADDING = 10;

  // Keycode for the "Escape" key.
  MarginTextbox.ESCAPE_KEYCODE = 27;
  // Keycode for the "Enter" key.
  MarginTextbox.ENTER_KEYCODE = 13;

  MarginTextbox.convertPointsToInchesText = function(toConvert) {
    var inInches = convertPointsToInches(toConvert);
    return MarginTextbox.convertInchesToInchesText(inInches);
  };

  MarginTextbox.convertInchesToInchesText = function(toConvert) {
    return toConvert.toFixed(2) + '"';
  };

  /**
   * @return {number} The total height of a margin textbox (including padding).
   */
  MarginTextbox.totalHeight = function() {
    return MarginTextbox.MARGIN_BOX_HEIGHT +
        2 * MarginTextbox.MARGIN_BOX_VERTICAL_PADDING;
  }

  /**
   * @return {number} The total width of a margin textbox (including padding).
   */
  MarginTextbox.totalWidth = function() {
    return MarginTextbox.MARGIN_BOX_WIDTH +
        2 * MarginTextbox.MARGIN_BOX_HORIZONTAL_PADDING;
  }

  MarginTextbox.prototype = {
    __proto__: HTMLInputElement.prototype,

    /**
     * Updates the state of |this|.
     * @param {print_preview.Rect} marginsRectangle A rectangle describing the
     *     margins in percentages.
     * @param {number} value The margin value in points.
     * @param {number} valueLimit The upper allowed value for the margin.
     * @param {boolean} keepDisplayedValue True if the currently displayed value
     *     should not be updated.
     */
    update: function(marginsRectangle, value, valueLimit, keepDisplayedValue) {
      this.marginsRectangle_ = marginsRectangle;
      this.lastValidValueInPoints = value;
      if (!keepDisplayedValue) {
        this.value = MarginTextbox.convertPointsToInchesText(
            this.lastValidValueInPoints);
      }

      this.valueLimit = valueLimit;
      this.validate();
    },

    get margin() {
      return print_preview.extractMarginValue(this.value);
    },

    /**
     * Updates |this.isValid|.
     */
    validate: function() {
      this.isValid = print_preview.isMarginTextValid(this.value,
                                                     this.valueLimit);
      if (this.isValid)
        this.value = MarginTextbox.convertInchesToInchesText(this.margin);
    },

    /**
     * Updates the background color depending on |isValid| by adding/removing
     * the appropriate CSS class.
     * @param {boolean} isValid True if the margin is valid.
     */
    updateColor: function() {
      this.isValid ? this.classList.remove('invalid') :
                     this.classList.add('invalid');
    },

    /**
     * Draws this textbox.
     */
    draw: function() {
      var topLeft = this.getCoordinates_();
      var totalWidth = previewArea.pdfPlugin_.offsetWidth;
      var totalHeight = previewArea.pdfPlugin_.offsetHeight;

      this.style.left = Math.round(topLeft.x * totalWidth) + 'px';
      this.style.top = Math.round(topLeft.y * totalHeight) + 'px';
      this.updateColor();
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
     * Calculates the coordinates where |this| should be displayed.
     * @return {{x: number, y: number}} The coordinates (in percent) where
     *     |this| should be drawn relative to the upper left corner of the
     *     plugin.
     * @private
     */
    getCoordinates_: function() {
      var x = 0, y = 0;
      var totalWidth = previewArea.pdfPlugin_.offsetWidth;
      var totalHeight = previewArea.pdfPlugin_.offsetHeight;
      var offsetY = (MarginTextbox.totalHeight() / 2) / totalHeight;
      var offsetX = (MarginTextbox.totalWidth() / 2) / totalWidth;

      if (this.isTop_()) {
        x = this.marginsRectangle_.middleX - offsetX;
        y = this.marginsRectangle_.y;
      } else if (this.isBottom_()) {
        x = this.marginsRectangle_.middleX - offsetX;
        y = this.marginsRectangle_.bottom - 2 * offsetY;
      } else if (this.isRight_()) {
        x = this.marginsRectangle_.right - 2 * offsetX;
        y = this.marginsRectangle_.middleY - offsetY;
      } else if (this.isLeft_()) {
        x = this.marginsRectangle_.x;
        y = this.marginsRectangle_.middleY - offsetY;
      }

      return { x: x, y: y };
    },

    /**
     * Adds event listeners for various events.
     * @private
     */
    addEventListeners_: function() {
      this.oninput = this.resetTimer_.bind(this);
      this.onblur = this.onBlur_.bind(this);
      this.onkeypress = this.onKeyPressed_.bind(this);
      this.onkeyup = this.onKeyUp_.bind(this);
    },

    /**
     * Executes whenever a blur event occurs.
     * @private
     */
    onBlur_: function() {
      clearTimeout(this.timerId_);
      this.validate();
      if (!this.isValid) {
        this.value = MarginTextbox.convertPointsToInchesText(
            this.lastValidValueInPoints);
        this.validate();
      }

      this.updateColor();
      cr.dispatchSimpleEvent(document, 'updateSummary');
      cr.dispatchSimpleEvent(document, 'updatePrintButton');
      cr.dispatchSimpleEvent(this, 'MarginsMayHaveChanged');
    },

    /**
     * Executes whenever a keypressed event occurs. Note: Only the "Enter" key
     * event is handled. The "Escape" key does not result in such event,
     * therefore it is handled by |this.onKeyUp_|.
     * @param {KeyboardEvent} e The event that triggered this listener.
     * @private
     */
    onKeyPressed_: function(e) {
      if (e.keyCode == MarginTextbox.ENTER_KEYCODE)
        this.blur();
    },

    /**
     * Executes whenever a keyup event occurs. Note: Only the "Escape"
     * key event is handled.
     * @param {KeyboardEvent} e The event that triggered this listener.
     * @private
     */
    onKeyUp_: function(e) {
      if (e.keyCode == MarginTextbox.ESCAPE_KEYCODE) {
        this.value = MarginTextbox.convertPointsToInchesText(
            this.lastValidValueInPoints);
        this.validate();
        this.updateColor();
        cr.dispatchSimpleEvent(document, 'updateSummary');
        cr.dispatchSimpleEvent(document, 'updatePrintButton');
      }
    },

    /**
     * Resetting the timer used to detect when the user stops typing in order
     * to update the print preview.
     * @private
     */
    resetTimer_: function() {
      clearTimeout(this.timerId_);
      this.timerId_ = window.setTimeout(
          this.onTextValueMayHaveChanged_.bind(this), 500);
    },

    /**
     * Executes whenever the user stops typing.
     * @private
     */
    onTextValueMayHaveChanged_: function() {
      this.validate();
      this.updateColor();
      cr.dispatchSimpleEvent(document, 'updateSummary');
      cr.dispatchSimpleEvent(document, 'updatePrintButton');

      if (!this.isValid)
        return;
      cr.dispatchSimpleEvent(this, 'MarginsMayHaveChanged');
    }

  };

  return {
    MarginTextbox: MarginTextbox
  };
});
