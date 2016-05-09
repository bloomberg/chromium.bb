// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-keyboard' is the settings subpage with keyboard settings.
 */

// TODO(michaelpg): The docs below are duplicates of settings_dropdown_menu,
// because we can't depend on settings_dropdown_menu in compiled_resources2.gyp
// withhout first converting settings_dropdown_menu to compiled_resources2.gyp.
// After the conversion, we should remove these.
/** @typedef {{name: string, value: (number|string)}} */
var DropdownMenuOption;
/** @typedef {!Array<!DropdownMenuOption>} */
var DropdownMenuOptionList;

/**
 * Auto-repeat delays (in ms) for the corresponding slider values, from
 * long to short. The values were chosen to provide a large range while giving
 * several options near the defaults.
 * @type {!Array<number>}
 * @const
 */
var AUTO_REPEAT_DELAYS =
    [2000, 1500, 1000, 500, 300, 200, 150];

/**
 * Auto-repeat intervals (in ms) for the corresponding slider values, from
 * long to short. The slider itself is labeled "rate", the inverse of
 * interval, and goes from slow (long interval) to fast (short interval).
 * @type {!Array<number>}
 * @const
 */
var AUTO_REPEAT_INTERVALS =
    [2000, 1000, 500, 300, 200, 100, 50, 30, 20];

var AUTO_REPEAT_DELAY_PREF = 'settings.language.xkb_auto_repeat_delay_r2';
var AUTO_REPEAT_INTERVAL_PREF = 'settings.language.xkb_auto_repeat_interval_r2';

Polymer({
  is: 'settings-keyboard',

  behaviors: [
    PrefsBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private Whether to show Caps Lock options. */
    showCapsLock_: Boolean,

    /** @private Whether to show diamond key options. */
    showDiamondKey_: Boolean,

    /** @private {!DropdownMenuOptionList} Menu items for key mapping. */
    keyMapTargets_: Object,

    /**
     * @private {!DropdownMenuOptionList} Menu items for key mapping, including
     * Caps Lock.
     */
    keyMapTargetsWithCapsLock_: Object,
  },

  observers: [
    'autoRepeatDelayPrefChanged_(' +
        'prefs.' + AUTO_REPEAT_DELAY_PREF + '.*)',
    'autoRepeatIntervalPrefChanged_(' +
        'prefs.' + AUTO_REPEAT_INTERVAL_PREF + '.*)',
  ],

  /** @override */
  ready: function() {
    cr.addWebUIListener('show-keys-changed', this.onShowKeysChange_.bind(this));
    chrome.send('initializeKeyboardSettings');
    this.setUpKeyMapTargets_();
  },

  /**
   * Initializes the dropdown menu options for remapping keys.
   * @private
   */
  setUpKeyMapTargets_: function() {
    this.keyMapTargets_ = [
      {value: 0, name: loadTimeData.getString('keyboardKeySearch')},
      {value: 1, name: loadTimeData.getString('keyboardKeyCtrl')},
      {value: 2, name: loadTimeData.getString('keyboardKeyAlt')},
      {value: 3, name: loadTimeData.getString('keyboardKeyDisabled')},
      {value: 5, name: loadTimeData.getString('keyboardKeyEscape')},
    ];

    var keyMapTargetsWithCapsLock = this.keyMapTargets_.slice();
    // Add Caps Lock, for keys allowed to be mapped to Caps Lock.
    keyMapTargetsWithCapsLock.splice(4, 0, {
      value: 4, name: loadTimeData.getString('keyboardKeyCapsLock'),
    });
    this.keyMapTargetsWithCapsLock_ = keyMapTargetsWithCapsLock;
  },

  /**
   * Handler for updating which keys to show.
   * @param {boolean} showCapsLock
   * @param {boolean} showDiamondKey
   * @private
   */
  onShowKeysChange_: function(showCapsLock, showDiamondKey) {
    this.showCapsLock_ = showCapsLock;
    this.showDiamondKey_ = showDiamondKey;
  },

  /** @private */
  autoRepeatDelayPrefChanged_: function() {
    var delay = /** @type number */(this.getPref(AUTO_REPEAT_DELAY_PREF).value);
    this.$.delaySlider.value =
        this.findNearestIndex_(AUTO_REPEAT_DELAYS, delay);
  },

  /** @private */
  autoRepeatIntervalPrefChanged_: function() {
    var interval = /** @type number */(
        this.getPref(AUTO_REPEAT_INTERVAL_PREF).value);
    this.$.repeatRateSlider.value =
        this.findNearestIndex_(AUTO_REPEAT_INTERVALS, interval);
  },

  /** @private */
  onDelaySliderChange_: function() {
    var index = this.$.delaySlider.value;
    assert(index >= 0 && index < AUTO_REPEAT_DELAYS.length);
    this.setPrefValue(AUTO_REPEAT_DELAY_PREF, AUTO_REPEAT_DELAYS[index]);
  },

  /** @private */
  onRepeatRateSliderChange_: function() {
    var index = this.$.repeatRateSlider.value;
    assert(index >= 0 && index < AUTO_REPEAT_INTERVALS.length);
    this.setPrefValue(AUTO_REPEAT_INTERVAL_PREF, AUTO_REPEAT_INTERVALS[index]);
  },

  /**
   * @return {number}
   * @private
   */
  delayMaxTick_: function() {
    return AUTO_REPEAT_DELAYS.length - 1;
  },

  /**
   * @return {number}
   * @private
   */
  repeatRateMaxTick_: function() {
    return AUTO_REPEAT_INTERVALS.length - 1;
  },

  /**
   * Returns the index of the item in |arr| closest to |value|. Same cost as
   * Array.prototype.indexOf if an exact match exists.
   * @param {!Array<number>} arr
   * @param {number} value
   * @return {number}
   * @private
   */
  findNearestIndex_: function(arr, value) {
    assert(arr.length);

    // The common case has an exact match.
    var closestIndex = arr.indexOf(value);
    if (closestIndex != -1)
      return closestIndex;

    // No exact match. Find the element closest to |value|.
    var minDifference = Number.MAX_VALUE;
    for (var i = 0; i < arr.length; i++) {
      var difference = Math.abs(arr[i] - value);
      if (difference < minDifference) {
        closestIndex = i;
        minDifference = difference;
      }
    }

    return closestIndex;
  },
});
