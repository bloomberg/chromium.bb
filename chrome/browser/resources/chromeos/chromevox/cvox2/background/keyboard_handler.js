// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox keyboard handler.
 */

goog.provide('BackgroundKeyboardHandler');

goog.require('cvox.ChromeVoxKbHandler');

/** @constructor */
BackgroundKeyboardHandler = function() {
  // Classic keymap.
  cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromDefaults();

  /** @type {number} @private */
  this.passThroughKeyUpCount_ = 0;

  /** @type {Set} @private */
  this.eatenKeyDowns_ = new Set();

  document.addEventListener('keydown', this.onKeyDown.bind(this), false);
  document.addEventListener('keyup', this.onKeyUp.bind(this), false);

  chrome.accessibilityPrivate.setKeyboardListener(true, false);
};

BackgroundKeyboardHandler.prototype = {
  /**
   * Handles key down events.
   * @param {Event} evt The key down event to process.
   * @return {boolean} True if the default action should be performed.
   */
  onKeyDown: function(evt) {
    evt.stickyMode = cvox.ChromeVox.isStickyModeOn() && cvox.ChromeVox.isActive;
    if (cvox.ChromeVox.passThroughMode)
      return false;

    if (ChromeVoxState.instance.mode != ChromeVoxMode.CLASSIC &&
        !cvox.ChromeVoxKbHandler.basicKeyDownActionsListener(evt)) {
      evt.preventDefault();
      evt.stopPropagation();
      this.eatenKeyDowns_.add(evt.keyCode);
    }
    Output.forceModeForNextSpeechUtterance(cvox.QueueMode.FLUSH);
    return false;
  },

  /**
   * Handles key up events.
   * @param {Event} evt The key down event to process.
   * @return {boolean} True if the default action should be performed.
   */
  onKeyUp: function(evt) {
    // Reset pass through mode once a keyup (not involving the pass through key)
    // is seen. The pass through command involves three keys.
    if (cvox.ChromeVox.passThroughMode) {
      if (this.passThroughKeyUpCount_ >= 3) {
        cvox.ChromeVox.passThroughMode = false;
        this.passThroughKeyUpCount_ = 0;
      } else {
        this.passThroughKeyUpCount_++;
      }
    }

    if (this.eatenKeyDowns_.has(evt.keyCode)) {
      evt.preventDefault();
      evt.stopPropagation();
      this.eatenKeyDowns_.delete(evt.keyCode);
    }

    return false;
  },

  /**
   * React to mode changes.
   * @param {ChromeVoxMode} newMode
   * @param {ChromeVoxMode?} oldMode
   */
  onModeChanged: function(newMode, oldMode) {
    if (newMode == ChromeVoxMode.CLASSIC) {
      chrome.accessibilityPrivate.setKeyboardListener(false, false);
    } else {
      chrome.accessibilityPrivate.setKeyboardListener(
          true, cvox.ChromeVox.isStickyPrefOn);
    }

    if (newMode === ChromeVoxMode.NEXT ||
        newMode === ChromeVoxMode.FORCE_NEXT) {
      // Switching out of classic, classic compat, or uninitialized
      // (on startup).
      window['prefs'].switchToKeyMap('keymap_next');
    } else if (
        oldMode && oldMode != ChromeVoxMode.CLASSIC &&
        oldMode != ChromeVoxMode.CLASSIC_COMPAT) {
      // Switching out of next. Intentionally do nothing when switching out of
      // an uninitialized |oldMode|.
      window['prefs'].switchToKeyMap('keymap_classic');
    }
  }
};
