// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common external variables when compiling ChromeVox background code

var localStorage = {};

/**
 * @type {Object}
 */
chrome.accessibilityPrivate = {};

/**
 * @param {boolean} on
 */
chrome.accessibilityPrivate.setAccessibilityEnabled = function(on) {};

/**
 * @param {boolean} on
 */
chrome.accessibilityPrivate.setNativeAccessibilityEnabled = function(on) {
};

/**
 * @param {boolean} enabled
 * @param {boolean} capture
 */
chrome.accessibilityPrivate.setKeyboardListener = function(enabled, capture) {
};

/**
 * @param {number} tabId
 * @param {function(Array<!Object>)} callback
 */
chrome.accessibilityPrivate.getAlertsForTab =
    function(tabId, callback) {};

/**
 * @param {Array<{left: number, top: number, width: number, height: number}>}
 *     rects The bounding rects to draw focus ring(s) around, in global
 *     screen coordinates.
 */
chrome.accessibilityPrivate.setFocusRing = function(rects) {
};

/** @type ChromeEvent */
chrome.accessibilityPrivate.onWindowOpened;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onWindowClosed;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onMenuOpened;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onMenuClosed;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onControlFocused;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onControlAction;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onControlHover;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onTextChanged;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onAccessibilityGesture;
