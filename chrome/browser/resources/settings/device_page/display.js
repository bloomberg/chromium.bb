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
     * Array of displays.
     * @type {!Array<!chrome.system.display.DisplayUnitInfo>}
     */
    displays: Array,

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

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @param {string} primaryDisplayId
   * @return {boolean}
   * @private
   */
  showMakePrimary_: function(selectedDisplay, primaryDisplayId) {
    return !!selectedDisplay && selectedDisplay.id != primaryDisplayId;
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @return {boolean}
   * @private
   */
  showMirror_: function(displays) {
    return this.isMirrored_(displays) || displays.length == 2;
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @return {boolean}
   * @private
   */
  isMirrored_: function(displays) {
    return displays.length > 0 && !!displays[0].mirroringSourceId;
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} display
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean}
   * @private
   */
  isSelected_: function(display, selectedDisplay) {
    return display.id == selectedDisplay.id;
  },

  /**
   * @param {!{model: !{index: number}}} e
   * @private
   */
  onSelectDisplayTap_: function(e) {
    this.selectedDisplay = this.displays[e.model.index];
  },

  /** @private */
  onMakePrimaryTap_: function() {
    if (!this.selectedDisplay)
      return;
    if (this.selectedDisplay.id == this.primaryDisplayId)
      return;
    /** @type {!chrome.system.display.DisplayProperties} */ var properties = {
      isPrimary: true
    };
    settings.display.systemDisplayApi.setDisplayProperties(
        this.selectedDisplay.id, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /**
   * @param {!{detail: !{selected: string}}} e
   * @private
   */
  onSetOrientation_: function(e) {
    /** @type {!chrome.system.display.DisplayProperties} */ var properties = {
      rotation: parseInt(e.detail.selected, 10)
    };
    settings.display.systemDisplayApi.setDisplayProperties(
        this.selectedDisplay.id, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /** @private */
  onMirroredTap_: function() {
    var id = '';
    /** @type {!chrome.system.display.DisplayProperties} */ var properties = {};
    if (this.isMirrored_(this.displays)) {
      id = this.primaryDisplayId;
      properties.mirroringSourceId = '';
    } else {
      // Set the mirroringSourceId of the secondary (first non-primary) display.
      for (var display of this.displays) {
        if (display.id != this.primaryDisplayId) {
          id = display.id;
          break;
        }
      }
      properties.mirroringSourceId = this.primaryDisplayId;
    }
    settings.display.systemDisplayApi.setDisplayProperties(
        id, properties, this.setPropertiesCallback_.bind(this));
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @private
   */
  updateDisplayInfo_(displays) {
    this.displays = displays;
    var primaryDisplay = undefined;
    var selectedDisplay = undefined;
    for (var display of this.displays) {
      if (display.isPrimary && !primaryDisplay)
        primaryDisplay = display;
      if (this.selectedDisplay && display.id == this.selectedDisplay.id)
        selectedDisplay = display;
    }
    this.primaryDisplayId = (primaryDisplay && primaryDisplay.id) || '';
    this.selectedDisplay = selectedDisplay || primaryDisplay ||
        (this.displays && this.displays[0]);
  },

  /** @private */
  setPropertiesCallback_: function() {
    if (chrome.runtime.lastError) {
      console.error(
          'setDisplayProperties Error: ' + chrome.runtime.lastError.message);
    }
  },
});
