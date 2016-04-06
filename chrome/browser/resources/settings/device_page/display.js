// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display' is the settings subpage for display settings.
 *
 * @group Chrome Settings Elements
 */

cr.define('settings.display', function() {
  var systemDisplayApi = /** @type {!SystemDisplay} */ (chrome.system.display);

  return {
    systemDisplayApi: systemDisplayApi,
  };
});

Polymer({
  is: 'settings-display',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * Map of displays by id.
     * @type {!Object<!chrome.system.display.DisplayUnitInfo>}
     */
    displays: Object,

    /** Primary display id */
    primaryDisplayId: String,

    /**
     * Selected display
     * @type {!chrome.system.display.DisplayUnitInfo|undefined}
     */
    selectedDisplay: Object,
  },

  /**
   * Listener for chrome.system.display.onDisplayChanged events.
   * @type {function(void)|undefined}
   * @private
   */
  displayChangedListener_: undefined,

  /** @override */
  attached: function() {
    this.displayChangedListener_ = this.getDisplayInfo_.bind(this);
    settings.display.systemDisplayApi.onDisplayChanged.addListener(
        this.displayChangedListener_);
    this.getDisplayInfo_();
  },

  /** @override */
  detached: function() {
    if (this.displayChangedListener_) {
      settings.display.systemDisplayApi.onDisplayChanged.removeListener(
          this.displayChangedListener_);
    }
  },

  /** @private */
  getDisplayInfo_: function() {
    settings.display.systemDisplayApi.getInfo(
        this.updateDisplayInfo_.bind(this));
  },

  /** @private */
  hasMultipleDisplays_: function(displays) {
    return Object.keys(displays).length > 1;
  },

  /** @private */
  isMirrored_: function(selectedDisplay) {
    return !!this.selectedDisplay.mirroringSourceId;
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displaysArray
   * @private
   */
  updateDisplayInfo_(displaysArray) {
    var displays = {};
    this.primaryDisplayId = '';
    for (var i = 0; i < displaysArray.length; ++i) {
      var display = displaysArray[i];
      displays[display.id] = display;
      if (display.isPrimary && !this.primaryDisplayId)
        this.primaryDisplayId = display.id;
    }
    this.displays = displays;
    // Always update selectedDisplay.
    this.selectedDisplay = this.displays[this.primaryDisplayId];
  },
});
