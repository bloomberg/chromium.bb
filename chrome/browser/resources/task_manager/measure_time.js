// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for measuring loading times.
 *
 * To be included as a first script in main.html
 */

/**
 * measureTime class
 * @constructor
 */
var measureTime = {
  isEnabled: localStorage.measureTimeEnabled,

  startInterval: function(name) {
    if (this.isEnabled)
      console.time(name);
  },

  recordInterval: function(name) {
    if (this.isEnabled)
      console.timeEnd(name);
  },
};

measureTime.startInterval('Load.Total');
measureTime.startInterval('Load.Script');
