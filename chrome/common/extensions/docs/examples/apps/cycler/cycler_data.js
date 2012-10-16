// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var CyclerData = function () {
  this.currentCaptures_ = ['alpha', 'beta', 'gamma'];

  /**
   * Mark a capture as saved successfully. Actual writing of the cache
   * directory and URL list into the FileSystem is done from the C++ side.
   * JS side just updates the capture choices.
   * @param {!string} name The name of the capture.
   * TODO(cstaley): Implement actual addition of new capture data
   */
  this.saveCapture = function(name) {
    console.log('Saving capture ' + name);
    this.currentCaptures_.push(name);
  }

  /**
   * Return a list of currently stored captures in the local FileSystem.
   * @return {Array.<!string>} Names of all the current captures.
   * TODO(cstaley): Implement actual generation of current capture list via
   * ls-like traversal of the extension's FileSystem.
   */
  this.getCaptures = function() {
    return this.currentCaptures_;
  }

  /**
   * Delete capture |name| from the local FileSystem, and update the
   * capture choices HTML select element.
   * @param {!string} name The name of the capture to delete.
   * TODO(cstaley): Implement actual deletion
   */
  this.deleteCapture = function(name) {
    this.currentCaptures_.splice(this.currentCaptures_.indexOf(name), 1);
  }
};
