// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Definitions for the Chromium extensions API used by ChromeVox.
 *
 * @externs
 */

/** @type {function() : !Object} */
chrome.app.getDetails;

// Media related automation actions and events.
chrome.automation.AutomationNode.prototype.resumeMedia = function() {};
chrome.automation.AutomationNode.prototype.startDuckingMedia = function() {};
chrome.automation.AutomationNode.prototype.stopDuckingMedia = function() {};
chrome.automation.AutomationNode.prototype.suspendMedia = function() {};
chrome.automation.EventType.mediaStartedPlaying;
chrome.automation.EventType.mediaStoppedPlaying;

/** @type {string|undefined} */
chrome.automation.AutomationNode.prototype.chromeChannel;
