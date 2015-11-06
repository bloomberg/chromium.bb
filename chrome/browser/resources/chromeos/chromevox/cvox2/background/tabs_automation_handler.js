// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation from a tabs automation node.
 */

goog.provide('TabsAutomationHandler');

goog.require('DesktopAutomationHandler');

/**
 * @param {!chrome.automation.AutomationNode} node
 * @constructor
 * @extends {DesktopAutomationHandler}
 */
TabsAutomationHandler = function(node) {
  DesktopAutomationHandler.call(this, node);
};

TabsAutomationHandler.prototype = {
  /** @override */
  didHandleEvent_: function(evt) {
    evt.stopPropagation();
  }
};
