// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File Description:
//     Contains all the necessary functions for rendering the Welcome page on
//     Android.

cr.define('about_welcome_android', function() {
  'use strict';

  /**
   * Disable context menus.
   */
  function initialize() {
    document.body.oncontextmenu = function(e) {
      e.preventDefault();
    }
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', about_welcome_android.initialize);
