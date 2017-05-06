// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface for controllers to interact with main SwitchAccess class.
 *
 * @interface
 */
function SwitchAccessInterface() {
  /**
   * @type {SwitchAccessPrefs}
   */
  this.switchAccessPrefs = null;
};

SwitchAccessInterface.prototype = {
  moveToNode: function(doNext) {},

  selectCurrentNode: function() {},

  showOptionsPage: function() {},

  performedUserAction: function() {},

  debugMoveToNext: function() {},

  debugMoveToPrevious: function() {},

  debugMoveToFirstChild: function() {},

  debugMoveToParent: function() {}
};
