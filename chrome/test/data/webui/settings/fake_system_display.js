// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.system.display for testing.
 */
cr.define('settings', function() {
  /**
   * Fake of the chrome.settings.display API.
   * @constructor
   * @implements {SystemDisplay}
   */
  function FakeSystemDisplay() {
    /** @type {!Array<!chrome.system.display.DisplayUnitInfo>} */
    this.fakeDisplays = [];
    this.getInfoCalled = new PromiseResolver();
  }

  FakeSystemDisplay.prototype = {
    // Public testing methods.
    /**
     * @param {!chrome.system.display.DisplayUnitInfo>} display
     */
    addDisplayForTest: function(display) {
      this.fakeDisplays.push(display);
    },

    // SystemDisplay overrides.
    /** @override */
    getInfo: function(callback) {
      setTimeout(function() {
        callback(this.fakeDisplays);
        this.getInfoCalled.resolve();
        // Reset the promise resolver.
        this.getInfoCalled = new PromiseResolver();
      }.bind(this));
    },

    /** @override */
    onDisplayChanged: new FakeChromeEvent(),
  };

  return {FakeSystemDisplay: FakeSystemDisplay};
});
