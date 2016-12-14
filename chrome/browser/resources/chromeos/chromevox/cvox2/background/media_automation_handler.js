// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles media events automation.
 */

goog.provide('MediaAutomationHandler');

goog.require('BaseAutomationHandler');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;

/**
 * @param {!AutomationNode} node The root to observe media changes.
 * @constructor
 * @extends {BaseAutomationHandler}
 */
MediaAutomationHandler = function(node) {
  BaseAutomationHandler.call(this, node);

  var e = EventType;
  this.addListener_(e.mediaStartedPlaying, this.onMediaStartedPlaying);
  this.addListener_(e.mediaStoppedPlaying, this.onMediaStoppedPlaying);
};

MediaAutomationHandler.prototype = {
  __proto__: BaseAutomationHandler.prototype,

  /**
   * @param {!AutomationEvent} evt
   */
  onMediaStartedPlaying: function(evt) {
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onMediaStoppedPlaying: function(evt) {
  }
};

});  // goog.scope
