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
    addDisplayForTest: function(display) { this.fakeDisplays.push(display); },

    // SystemDisplay overrides.
    /** @override */
    getInfo: function(callback) {
      setTimeout(function() {
        if (this.fakeDisplays.length > 0 &&
            this.fakeDisplays[0].mirroringSourceId) {
          // When mirroring is enabled, send only the info for the display
          // being mirrored.
          var display =
              this.getFakeDisplay_(this.fakeDisplays[0].mirroringSourceId);
          assert(!!display);
          callback([display]);
        } else {
          callback(this.fakeDisplays);
        }
        this.getInfoCalled.resolve();
        // Reset the promise resolver.
        this.getInfoCalled = new PromiseResolver();
      }.bind(this));
    },

    /** @override */
    setDisplayProperties: function(id, info, callback) {
      var display = this.getFakeDisplay_(id);
      if (!display) {
        chrome.runtime.lastError = 'Display not found.';
        callback();
      }

      if (info.mirroringSourceId != undefined) {
        for (var d of this.fakeDisplays)
          d.mirroringSourceId = info.mirroringSourceId;
      }

      if (info.isPrimary != undefined) {
        var havePrimary = info.isPrimary;
        for (var d of this.fakeDisplays) {
          if (d.id == id) {
            d.isPrimary = info.isPrimary;
          } else if (havePrimary) {
            d.isPrimary = false;
          } else {
            d.isPrimary = true;
            havePrimary = true;
          }
        }
      }
      if (info.rotation != undefined)
        display.rotation = info.rotation;
    },

    /** @override */
    onDisplayChanged: new FakeChromeEvent(),

    /** @private */
    getFakeDisplay_(id) {
      var idx = this.fakeDisplays.findIndex(function(display) {
        return display.id == id;
      });
      if (idx >= 0)
        return this.fakeDisplays[idx];
      return undefined;
    }
  };

  return {FakeSystemDisplay: FakeSystemDisplay};
});
