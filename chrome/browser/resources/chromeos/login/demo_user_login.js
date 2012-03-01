// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Demo login UI.
 */

/**
 * Handles a user clicking anywhere on the screen. This will log the demo user
 * in. Yes, this actually _is the intention.
 */
onClick = function() {
  chrome.send('launchDemoUser');
};

/**
 * Initializes the click handler.
 */
initialize = function() {
  document.addEventListener('click', onClick);
};

disableTextSelectAndDrag();
document.addEventListener('DOMContentLoaded', initialize);
