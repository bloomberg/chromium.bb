// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A drop-in replacement for one method that we could use from Closure.

goog.provide('goog.style');

/**
 * Creates a style sheet in the document containing the passed rules.
 * A simplified version that does not take an optional node parameter.
 * @param {string} rules
 */
goog.style.installStyles = function(rules) {
  try {
    var ss = document.createElement('style');
    ss.setAttribute('type', 'text/css');
    if (ss.styleSheet) {
      ss.styleSheet.cssText = rules;
    } else {
      ss.appendChild(document.createTextNode(rules));
    }
    var h = document.getElementsByTagName('head')[0];
    var firstChild = h.firstChild;
    h.insertBefore(ss, firstChild);
  } catch (e) {
    // squelch
  }
};
