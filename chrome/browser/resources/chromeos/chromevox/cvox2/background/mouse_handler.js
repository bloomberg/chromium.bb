// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox mouse handler.
 */

goog.provide('BackgroundMouseHandler');

goog.require('BaseAutomationHandler');

var AutomationEvent = chrome.automation.AutomationEvent;
var EventType = chrome.automation.EventType;

/**
 * @constructor
 * @extends {BaseAutomationHandler}
 */
BackgroundMouseHandler = function() {
  chrome.automation.getDesktop(function(desktop) {
    BaseAutomationHandler.call(this, desktop);
    /**
     *
     * The desktop node.
     *
     * @private {!chrome.automation.AutomationNode}
     */
    this.desktop_ = desktop;
    this.addListener_(EventType.MOUSE_MOVED, this.onMouseMove);
  }.bind(this));
};

BackgroundMouseHandler.prototype = {
  __proto__: BaseAutomationHandler.prototype,

  /**
   * Handles mouse move events.
   * @param {AutomationEvent} evt The mouse move event to process.
   */
  onMouseMove: function(evt) {
    this.desktop_.hitTest(evt.mouseX, evt.mouseY, EventType.HOVER);
  },
};
