// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox mouse handler.
 */

goog.provide('BackgroundMouseHandler');

var EventType = chrome.automation.EventType;

/** @constructor */
BackgroundMouseHandler = function() {
  document.addEventListener('mousemove', this.onMouseMove.bind(this));

  chrome.automation.getDesktop(function(desktop) {
    /**
     *
     * The desktop node.
     *
     * @private {!chrome.automation.AutomationNode}
     */
    this.desktop_ = desktop;
  }.bind(this));
};

BackgroundMouseHandler.prototype = {
  /**
   * Handles mouse move events.
   * @param {Event} evt The mouse move event to process.
   * @return {boolean} True if the default action should be performed.
   */
  onMouseMove: function(evt) {
    // Immediately save the most recent mouse coordinates.
    this.desktop_.hitTest(evt.screenX, evt.screenY, EventType.HOVER);
    return false;
  },
};
