// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation from a tabs automation node.
 */

goog.provide('TabsAutomationHandler');

goog.require('CustomAutomationEvent');
goog.require('DesktopAutomationHandler');

goog.scope(function() {
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;

/**
 * @param {!chrome.automation.AutomationNode} tabRoot
 * @constructor
 * @extends {DesktopAutomationHandler}
 */
TabsAutomationHandler = function(tabRoot) {
  DesktopAutomationHandler.call(this, tabRoot);

  if (tabRoot.role != RoleType.ROOT_WEB_AREA)
    throw new Error('Expected rootWebArea node but got ' + tabRoot.role);

  // When the root is focused, simulate what happens on a load complete.
  if (tabRoot.state[StateType.FOCUSED]) {
    var event = new CustomAutomationEvent(
        EventType.LOAD_COMPLETE, tabRoot, 'page');
    this.onLoadComplete(event);
  }
};

TabsAutomationHandler.prototype = {
  __proto__: DesktopAutomationHandler.prototype,

  /** @override */
  didHandleEvent_: function(evt) {
    evt.stopPropagation();
  },

  /** @override */
  onLoadComplete: function(evt) {
    var focused = evt.target.find({state: {focused: true}}) || evt.target;
    var event = new CustomAutomationEvent(
        EventType.FOCUS, focused, evt.eventFrom);
    this.onFocus(event);
  }
};

});  // goog.scope
