// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
    box.setAttribute('aria-label', localStrings.getString(groupName));

    // @type {string} Specifies which margin this line refers to.
    box.marginGroup = groupName;
    // @type {boolean} True if the displayed value is valid.
    box.isValid = true;
    // @type {number} Timer used to detect when the user stops typing.
    box.timerId_ = null;
    // @type {number} The last valid value in points.
    box.lastValidValueInPoints = 0;
    // @type {number} The upper allowed limit for the corresponding margin.
    box.valueLimit = null;

    box.addEventListeners_();
    return box;
  }

  MarginTextbox.CSS_CLASS_MARGIN_TEXTBOX = 'margin-box';
  // Keycode for the "Escape" key.
  MarginTextbox.ESCAPE_KEYCODE = 27;

  MarginTextbox.prototype = {
    __proto__: HTMLInputElement.prototype,

    /**
     * @type {number} The margin value currently in the textbox.
     */
    get margin() {
      return print_preview.extractMarginValue(this.value);
    },

    /**
     * Sets the contents of the textbox.
     * @param {number} newValueInPoints The value to be displayed in points.
     * @private
     */
    setValue_: function(newValueInPoints) {
      this.value =
          print_preview.convertPointsToLocaleUnitsText(newValueInPoints);
    },

    /**
     * Updates the state of |this|.
     * @param {number} value The margin value in points.
     * @param {number} valueLimit The upper allowed value for the margin.
     * @param {boolean} keepDisplayedValue True if the currently displayed value
     *     should not be updated.
     */
    update: function(value, valueLimit, keepDisplayedValue) {
      this.lastValidValueInPoints = value;
      if (!keepDisplayedValue)
        this.setValue_(this.lastValidValueInPoints);

      this.valueLimit = valueLimit;
      this.validate();
    },

    /**
     * Updates |this| while dragging is in progress.
     * @param {number} dragDeltaInPoints The difference in points between the
     *     margin value before dragging started and now.
     */
    updateWhileDragging: function(dragDeltaInPoints) {
      var validity = this.validateDelta(dragDeltaInPoints);

      if (validity == print_preview.marginValidationStates.WITHIN_RANGE)
        this.setValue_(this.lastValidValueInPoints + dragDeltaInPoints);
      else if (validity == print_preview.marginValidationStates.TOO_SMALL)
        this.setValue_(0);
      else if (validity == print_preview.marginValidationStates.TOO_BIG)
        this.setValue_(this.valueLimit);

      this.validate();
      this.updateColor();
    },

    /**
     * @param {number} dragDeltaInPoints The difference in points between the
     *     margin value before dragging started and now.
     * @return {number} An appropriate value from enum |marginValidationStates|.
     */
    validateDelta: function(dragDeltaInPoints) {
      var newValue = this.lastValidValueInPoints + dragDeltaInPoints;
      return print_preview.validateMarginValue(newValue, this.valueLimit);
    },

    /**
     * Updates |this.isValid|.
     */
    validate: function() {
      this.isValid =
          print_preview.validateMarginText(this.value, this.valueLimit) ==
              print_preview.marginValidationStates.WITHIN_RANGE;
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
      this.updateColor();
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
      this.onfocus = function() {
        cr.dispatchSimpleEvent(document, customEvents.MARGIN_TEXTBOX_FOCUSED);
      };
    },

    /**
     * Executes whenever a blur event occurs.
     * @private
     */
    onBlur_: function() {
      clearTimeout(this.timerId_);
      this.validate();
      if (!this.isValid) {
        this.setValue_(this.lastValidValueInPoints);
        this.validate();
      }

      this.updateColor();
      cr.dispatchSimpleEvent(document, customEvents.UPDATE_SUMMARY);
      cr.dispatchSimpleEvent(document, customEvents.UPDATE_PRINT_BUTTON);
      cr.dispatchSimpleEvent(this, customEvents.MARGINS_MAY_HAVE_CHANGED);
    },

    /**
     * Executes whenever a keypressed event occurs. Note: Only the "Enter" key
     * event is handled. The "Escape" key does not result in such event,
     * therefore it is handled by |this.onKeyUp_|.
     * @param {KeyboardEvent} e The event that triggered this listener.
     * @private
     */
    onKeyPressed_: function(e) {
      if (e.keyIdentifier == 'Enter')
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
        this.setValue_(this.lastValidValueInPoints);
        this.validate();
        this.updateColor();
        cr.dispatchSimpleEvent(document, customEvents.UPDATE_SUMMARY);
        cr.dispatchSimpleEvent(document, customEvents.UPDATE_PRINT_BUTTON);
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
          this.onTextValueMayHaveChanged.bind(this), 1000);
    },

    /**
     * Executes whenever the user stops typing or when a drag session associated
     * with |this| ends.
     */
    onTextValueMayHaveChanged: function() {
      this.validate();
      this.updateColor();
      cr.dispatchSimpleEvent(document, customEvents.UPDATE_SUMMARY);
      cr.dispatchSimpleEvent(document, customEvents.UPDATE_PRINT_BUTTON);

      if (!this.isValid)
        return;
      cr.dispatchSimpleEvent(this, customEvents.MARGINS_MAY_HAVE_CHANGED);
    }

  };

  return {
    MarginTextbox: MarginTextbox
  };
});
