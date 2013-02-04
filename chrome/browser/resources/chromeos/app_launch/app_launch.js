// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App install/launch splash screen.
 */

/**
 * Initializes the click handler.
 */
initialize = function() {
  $('page').style.opacity = 1;
  // Report back sign in UI being painted.
  window.webkitRequestAnimationFrame(function() {});
};

disableTextSelectAndDrag();
document.addEventListener('DOMContentLoaded', initialize);
