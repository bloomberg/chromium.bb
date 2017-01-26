// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display' is the settings subpage for display settings.
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

    /**
     * Array of display layouts.
     * @type {!Array<!chrome.system.display.DisplayLayout>}
     */
    layouts: Array,

    /**
     * String listing the ids in displays. Used to observe changes to the
     * display configuration (i.e. when a display is added or removed).
     */
    displayIds: {type: String, observer: 'onDisplayIdsChanged_'},

    /** Primary display id */
    primaryDisplayId: String,

    /** @type {!chrome.system.display.DisplayUnitInfo|undefined} */
    selectedDisplay: {type: Object, observer: 'selectedDisplayChanged_'},

    /** Id passed to the overscan dialog. */
    overscanDisplayId: {
      type: String,
      notify: true,
    },

    /** @private {!Array<number>} Mode index values for slider. */
    modeValues_: Array,

    /** @private Selected mode index value for slider. */
    selectedModeIndex_: Number,
  },

  /** @private {number} Selected mode index received from chrome. */
  currentSelectedModeIndex_: -1,

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
    this.currentSelectedModeIndex_ = -1;
  },

  /**
   * Shows or hides the overscan dialog.
   * @param {boolean} showOverscan
   * @private
   */
  showOverscanDialog_: function(showOverscan) {
    if (showOverscan) {
      this.$.displayOverscan.open();
      this.$.displayOverscan.focus();
    } else {
      this.$.displayOverscan.close();
    }
  },

  /** @private */
  onDisplayIdsChanged_: function() {
    // Close any overscan dialog (which will cancel any overscan operation)
    // if displayIds changes.
    this.showOverscanDialog_(false);
  },

  /** @private */
  getDisplayInfo_: function() {
    settings.display.systemDisplayApi.getInfo(
        this.displayInfoFetched_.bind(this));
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @private
   */
  displayInfoFetched_: function(displays) {
    if (!displays.length)
      return;
    settings.display.systemDisplayApi.getDisplayLayout(
        this.displayLayoutFetched_.bind(this, displays));
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @param {!Array<!chrome.system.display.DisplayLayout>} layouts
   * @private
   */
  displayLayoutFetched_: function(displays, layouts) {
    this.layouts = layouts;
    this.displays = displays;
    this.updateDisplayInfo_();
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {number}
   * @private
   */
  getSelectedModeIndex_: function(selectedDisplay) {
    for (var i = 0; i < selectedDisplay.modes.length; ++i) {
      if (selectedDisplay.modes[i].isSelected)
        return i;
    }
    return 0;
  },

  /** @private */
  selectedDisplayChanged_: function() {
    // Set |modeValues_| before |selectedModeIndex_| so that the slider updates
    // correctly.
    var numModes = this.selectedDisplay.modes.length;
    if (numModes == 0) {
      this.modeValues_ = [];
      this.selectedModeIndex_ = 0;
      this.currentSelectedModeIndex_ = 0;
      return;
    }
    this.modeValues_ = Array.from(Array(numModes).keys());
    this.selectedModeIndex_ = this.getSelectedModeIndex_(this.selectedDisplay);
    this.currentSelectedModeIndex_ = this.selectedModeIndex_;
  },

  /**
   * Returns true if the given display has touch support and is not an internal
   * display. If the feature is not enabled via the switch, this will return
   * false.
   * @param {!chrome.system.display.DisplayUnitInfo} display Display being
   *     checked for touch support.
   * @return {boolean}
   * @private
   */
  showTouchCalibrationSetting_: function(display) {
    return !display.isInternal && display.hasTouchSupport &&
        loadTimeData.getBoolean('enableTouchCalibrationSetting');
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @return {boolean}
   * @private
   */
  hasMultipleDisplays_: function(displays) {
    return displays.length > 1;
  },

  /**
   * Returns false if the display select menu has to be hidden.
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean}
   * @private
   */
  showDisplaySelectMenu_: function(displays, selectedDisplay) {
    return displays.length > 1 && !selectedDisplay.isPrimary;
  },

  /**
   * Returns the select menu index indicating whether the display currently is
   * primary or extended.
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @param {string} primaryDisplayId
   * @return {number} Retruns 0 if the display is primary else returns 1.
   * @private
   */
  getDisplaySelectMenuIndex_: function(selectedDisplay, primaryDisplayId) {
    if (selectedDisplay && selectedDisplay.id == primaryDisplayId)
      return 0;
    return 1;
  },

  /**
   * Returns the i18n string for the text to be used for mirroring settings.
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @return {string} i18n string for mirroring settings text.
   * @private
   */
  getDisplayMirrorText_: function(displays) {
    return this.i18n(
        this.isMirrored_(displays) ? 'displayMirrorOn' : 'displayMirrorOff');
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
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean}
   * @private
   */
  enableSetResolution_: function(selectedDisplay) {
    return selectedDisplay.modes.length > 1;
  },

  /**
   * @return {string}
   * @private
   */
  getResolutionText_: function() {
    if (this.selectedDisplay.modes.length == 0 ||
        this.currentSelectedModeIndex_ == -1) {
      // If currentSelectedModeIndex_ == -1, selectedDisplay and
      // selectedModeIndex_ are not in sync.
      return this.i18n(
          'displayResolutionText', this.selectedDisplay.bounds.width.toString(),
          this.selectedDisplay.bounds.height.toString());
    }
    var mode = this.selectedDisplay.modes[this.selectedModeIndex_];
    var best =
        this.selectedDisplay.isInternal ? mode.uiScale == 1.0 : mode.isNative;
    var widthStr = mode.width.toString();
    var heightStr = mode.height.toString();
    if (best)
      return this.i18n('displayResolutionTextBest', widthStr, heightStr);
    else if (mode.isNative)
      return this.i18n('displayResolutionTextNative', widthStr, heightStr);
    return this.i18n('displayResolutionText', widthStr, heightStr);
  },

  /**
   * @param {!{detail: string}} e |e.detail| is the id of the selected display.
   * @private
   */
  onSelectDisplay_: function(e) {
    var id = e.detail;
    for (var i = 0; i < this.displays.length; ++i) {
      var display = this.displays[i];
      if (id != display.id)
        continue;
      if (this.selectedDisplay != display) {
        // Set currentSelectedModeIndex_ to -1 so that getResolutionText_ does
        // not flicker. selectedDisplayChanged will update selectedModeIndex_
        // and currentSelectedModeIndex_ correctly.
        this.currentSelectedModeIndex_ = -1;
        this.selectedDisplay = display;
      }
    }
  },

  /**
   * Handles event when a display tab is selected.
   * @param {!{detail: !{item: !{displayId: string}}}} e
   * @private
   */
  onSelectDisplayTab_: function(e) {
    this.onSelectDisplay_({detail: e.detail.item.displayId});
  },

  /**
   * Handles event when a touch calibration option is selected.
   * @param {!Event} e
   * @private
   */
  onTouchCalibrationTap_: function(e) {
    settings.display.systemDisplayApi.showNativeTouchCalibration(
        this.selectedDisplay.id);
  },

  /**
   * Handles the event when an option from display select menu is selected.
   * @param {!{target: !HTMLSelectElement}} e
   * @private
   */
  updatePrimaryDisplay_: function(e) {
    /** @const {number} */ var PRIMARY_DISP_IDX = 0;
    if (!this.selectedDisplay)
      return;
    if (this.selectedDisplay.id == this.primaryDisplayId)
      return;
    if (e.target.value != PRIMARY_DISP_IDX)
      return;

    /** @type {!chrome.system.display.DisplayProperties} */ var properties = {
      isPrimary: true
    };
    settings.display.systemDisplayApi.setDisplayProperties(
        this.selectedDisplay.id, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /**
   * Triggered when the 'change' event for the selected mode slider is
   * triggered. This only occurs when the value is comitted (i.e. not while
   * the slider is being dragged).
   * @private
   */
  onSelectedModeChange_: function() {
    if (this.currentSelectedModeIndex_ == -1 ||
        this.currentSelectedModeIndex_ == this.selectedModeIndex_) {
      // Don't change the selected display mode until we have received an update
      // from Chrome and the mode differs from the current mode.
      return;
    }
    /** @type {!chrome.system.display.DisplayProperties} */ var properties = {
      displayMode: this.selectedDisplay.modes[this.selectedModeIndex_]
    };
    settings.display.systemDisplayApi.setDisplayProperties(
        this.selectedDisplay.id, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onOrientationChange_: function(event) {
    var target = /** @type {!HTMLSelectElement} */ (event.target);
    /** @type {!chrome.system.display.DisplayProperties} */ var properties = {
      rotation: parseInt(target.value, 10)
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
      for (var i = 0; i < this.displays.length; ++i) {
        var display = this.displays[i];
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
   * @param {!Event} e
   * @private
   */
  onOverscanTap_: function(e) {
    e.preventDefault();
    this.overscanDisplayId = this.selectedDisplay.id;
    this.showOverscanDialog_(true);
  },

  /** @private */
  updateDisplayInfo_: function() {
    var displayIds = '';
    var primaryDisplay = undefined;
    var selectedDisplay = undefined;
    for (var i = 0; i < this.displays.length; ++i) {
      var display = this.displays[i];
      if (displayIds)
        displayIds += ',';
      displayIds += display.id;
      if (display.isPrimary && !primaryDisplay)
        primaryDisplay = display;
      if (this.selectedDisplay && display.id == this.selectedDisplay.id)
        selectedDisplay = display;
    }
    this.displayIds = displayIds;
    this.primaryDisplayId = (primaryDisplay && primaryDisplay.id) || '';
    this.selectedDisplay = selectedDisplay || primaryDisplay ||
        (this.displays && this.displays[0]);
    // Save the selected mode index received from Chrome so that we do not
    // send an unnecessary setDisplayProperties call (which would log an error).
    this.currentSelectedModeIndex_ =
        this.getSelectedModeIndex_(this.selectedDisplay);

    this.$.displayLayout.updateDisplays(this.displays, this.layouts);
  },

  /** @private */
  setPropertiesCallback_: function() {
    if (chrome.runtime.lastError) {
      console.error(
          'setDisplayProperties Error: ' + chrome.runtime.lastError.message);
    }
  },
});
