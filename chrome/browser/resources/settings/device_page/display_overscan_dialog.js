// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display-overscan-dialog' is the dialog for display overscan
 * adjustments.
 */

Polymer({
  is: 'settings-display-overscan-dialog',

  properties: {
    /** Id of the display for which overscan is being applied (or empty). */
    displayId: {
      type: String,
      notify: true,
      observer: 'displayIdChanged_',
    },

    /** Set to true once changes are saved to avoid a reset/cancel on close. */
    comitted_: Boolean,
  },

  /**
   * Keyboard event handler for overscan adjustments.
   * @type {?function(!Event)}
   * @private
   */
  keyHandler_: null,

  open: function() {
    this.keyHandler_ = this.handleKeyEvent_.bind(this);
    // We need to attach the event listener to |window|, not |this| so that
    // changing focus does not prevent key events from occuring.
    window.addEventListener('keydown', this.keyHandler_);
    this.comitted_ = false;
    this.$.dialog.showModal();
    // Don't focus 'reset' by default. 'Tab' will focus 'OK'.
    this.$$('#reset').blur();
  },

  close: function() {
    window.removeEventListener('keydown', this.keyHandler_);

    this.displayId = '';  // Will trigger displayIdChanged_.

    if (this.$.dialog.open)
      this.$.dialog.close();
  },

  /** @private */
  displayIdChanged_: function(newValue, oldValue) {
    if (oldValue && !this.comitted_) {
      settings.display.systemDisplayApi.overscanCalibrationReset(oldValue);
      settings.display.systemDisplayApi.overscanCalibrationComplete(oldValue);
    }
    if (!newValue)
      return;
    this.comitted_ = false;
    settings.display.systemDisplayApi.overscanCalibrationStart(newValue);
  },

  /** @private */
  onResetTap_: function() {
    settings.display.systemDisplayApi.overscanCalibrationReset(this.displayId);
  },

  /** @private */
  onSaveTap_: function() {
    settings.display.systemDisplayApi.overscanCalibrationComplete(
        this.displayId);
    this.comitted_ = true;
    this.close();
  },

  /**
   * @param {!Event} event
   * @private
   */
  handleKeyEvent_: function(event) {
    if (event.altKey || event.ctrlKey || event.metaKey)
      return;
    switch (event.keyCode) {
      case 37:  // left arrow
        if (event.shiftKey)
          this.move_(-1, 0);
        else
          this.resize_(1, 0);
        break;
      case 38:  // up arrow
        if (event.shiftKey)
          this.move_(0, -1);
        else
          this.resize_(0, -1);
        break;
      case 39:  // right arrow
        if (event.shiftKey)
          this.move_(1, 0);
        else
          this.resize_(-1, 0);
        break;
      case 40:  // down arrow
        if (event.shiftKey)
          this.move_(0, 1);
        else
          this.resize_(0, 1);
        break;
      default:
        // Allow unhandled key events to propagate.
        return;
    }
    event.preventDefault();
  },

  /**
   * @param {number} x
   * @param {number} y
   * @private
   */
  move_: function(x, y) {
    /** @type {!chrome.system.display.Insets} */ var delta = {
      left: x,
      top: y,
      right: x ? -x : 0,  // negating 0 will produce a double.
      bottom: y ? -y : 0,
    };
    settings.display.systemDisplayApi.overscanCalibrationAdjust(
        this.displayId, delta);
  },

  /**
   * @param {number} x
   * @param {number} y
   * @private
   */
  resize_: function(x, y) {
    /** @type {!chrome.system.display.Insets} */ var delta = {
      left: x,
      top: y,
      right: x,
      bottom: y,
    };
    settings.display.systemDisplayApi.overscanCalibrationAdjust(
        this.displayId, delta);
  }
});
