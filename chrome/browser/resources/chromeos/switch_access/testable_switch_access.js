// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Subclass of SwitchAccess that exposes SwitchAccess's methods for testing.
 */
let TestableSwitchAccess = function() {
  SwitchAccess.call(this);
};

TestableSwitchAccess.prototype = {
  __proto__: SwitchAccess.prototype,

  init_: function() {},

  getNextNode: function(node) {
    return this.getNextNode_(node);
  },

  getPreviousNode: function(node) {
    return this.getPreviousNode_(node);
  },

  getYoungestDescendant: function(node) {
    return this.getYoungestDescendant_(node);
  },

  isInteresting: function(node) {
    return this.isInteresting_(node);
  }
};
