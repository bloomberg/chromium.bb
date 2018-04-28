// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles gesture-based commands.
 */

goog.provide('GestureCommandHandler');

goog.require('CommandHandler');
goog.require('EventSourceState');
goog.require('GestureCommandData');

/**
 * Global setting for the enabled state of this handler.
 * @param {boolean} state
 */
GestureCommandHandler.setEnabled = function(state) {
  GestureCommandHandler.enabled_ = state;
};

/**
 * Global setting for the enabled state of this handler.
 * @return {boolean}
 */
GestureCommandHandler.getEnabled = function() {
  return GestureCommandHandler.enabled_;
};

/**
 * Handles accessibility gestures from the touch screen.
 * @param {string} gesture The gesture to handle, based on the
 *     ax::mojom::Gesture enum defined in ui/accessibility/ax_enums.mojom
 * @private
 */
GestureCommandHandler.onAccessibilityGesture_ = function(gesture) {
  if (!GestureCommandHandler.enabled_ || !ChromeVoxState.instance.currentRange)
    return;

  EventSourceState.set(EventSourceType.TOUCH_GESTURE);

  var commandData = GestureCommandData.GESTURE_COMMAND_MAP[gesture];
  if (!commandData)
    return;

  Output.forceModeForNextSpeechUtterance(cvox.QueueMode.FLUSH);
  var textEditHandler = DesktopAutomationHandler.instance.textEditHandler;
  if (textEditHandler && commandData.keyOverride) {
    var key = commandData.keyOverride;
    if (!key.multiline ||
        ((!key.skipStart || !textEditHandler.isSelectionOnFirstLine()) &&
         (!key.skipEnd || !textEditHandler.isSelectionOnLastLine()))) {
      BackgroundKeyboardHandler.sendKeyPress(key.keyCode, key.modifiers);
      return;
    }
  }

  CommandHandler.onCommand(commandData.command);
};

/** @private {boolean} */
GestureCommandHandler.enabled_ = true;

/** Performs global setup. @private */
GestureCommandHandler.init_ = function() {
  chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
      GestureCommandHandler.onAccessibilityGesture_);
};

GestureCommandHandler.init_();
