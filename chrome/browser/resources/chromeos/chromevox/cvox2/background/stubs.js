// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Installs stubs for unsupported api's on some platforms. Define
 * the stubs here and require Stubs in the calling file.
 */

goog.provide('Stubs');

goog.require('cvox.ChromeVox');

/** Initializes this module. */
Stubs.init = function() {
  // These api's throw for non-Chrome OS.
  if (!cvox.ChromeVox.isChromeOS) {
    chrome.accessibilityPrivate.onAccessibilityGesture = {};
    chrome.accessibilityPrivate.onAccessibilityGesture.addListener =
        function() {};
    chrome.accessibilityPrivate.setFocusRing = function() {};
    chrome.accessibilityPrivate.setKeyboardListener = function() {};
  }

  // This api throws on Mac.
  if (cvox.ChromeVox.isMac) {
    chrome.automation.getDesktop = function() {};
  }

  // Missing api's until 53.
  if (!chrome.automation.NameFromType) {
    chrome.automation['NameFromType'] = {};
    chrome.automation['DescriptionFromType'] = {};
  }
};

Stubs.init();
