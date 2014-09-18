// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  /**
   * Auto-repeat delays (in ms) for the corresponding slider values, from
   * long to short. The values were chosen to provide a large range while giving
   * several options near the defaults.
   * @type {!Array.<number>}
   * @const
   */
  var AUTO_REPEAT_DELAYS =
      [2000, 1500, 1000, 500, 300, 200, 150];

  /**
   * Auto-repeat intervals (in ms) for the corresponding slider values, from
   * long to short. The slider itself is labeled "rate", the inverse of
   * interval, and goes from slow (long interval) to fast (short interval).
   * @type {!Array.<number>}
   * @const
   */
  var AUTO_REPEAT_INTERVALS =
      [2000, 1000, 500, 300, 200, 100, 50, 30, 20];

  /**
   * Encapsulated handling of the keyboard overlay.
   * @constructor
   * @extends {options.SettingsDialog}
   */
  function KeyboardOverlay() {
    options.SettingsDialog.call(this, 'keyboard-overlay',
        loadTimeData.getString('keyboardOverlayTabTitle'),
        'keyboard-overlay',
        assertInstanceof($('keyboard-confirm'), HTMLButtonElement),
        assertInstanceof($('keyboard-cancel'), HTMLButtonElement));
  }

  cr.addSingletonGetter(KeyboardOverlay);

  KeyboardOverlay.prototype = {
    __proto__: options.SettingsDialog.prototype,

    /** @override */
    initializePage: function() {
      options.SettingsDialog.prototype.initializePage.call(this);

      $('enable-auto-repeat').customPrefChangeHandler =
          this.handleAutoRepeatEnabledPrefChange_.bind(this);

      var autoRepeatDelayRange = $('auto-repeat-delay-range');
      autoRepeatDelayRange.valueMap = AUTO_REPEAT_DELAYS;
      autoRepeatDelayRange.max = AUTO_REPEAT_DELAYS.length - 1;
      autoRepeatDelayRange.customPrefChangeHandler =
          this.handleAutoRepeatDelayPrefChange_.bind(this);

      var autoRepeatIntervalRange = $('auto-repeat-interval-range');
      autoRepeatIntervalRange.valueMap = AUTO_REPEAT_INTERVALS;
      autoRepeatIntervalRange.max = AUTO_REPEAT_INTERVALS.length - 1;
      autoRepeatIntervalRange.customPrefChangeHandler =
          this.handleAutoRepeatIntervalPrefChange_.bind(this);

      $('languages-and-input-settings').onclick = function(e) {
        PageManager.showPageByName('languages');
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_KeyboardShowLanguageSettings']);
      };

      $('keyboard-shortcuts').onclick = function(e) {
        chrome.send('showKeyboardShortcuts');
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_KeyboardShowKeyboardShortcuts']);
      };
    },

    /**
     * Handles auto-repeat enabled pref change and allows the event to continue
     * propagating.
     * @param {Event} e Change event.
     * @return {boolean} Whether the event has finished being handled.
     * @private
     */
    handleAutoRepeatEnabledPrefChange_: function(e) {
      $('auto-repeat-settings-section').classList.toggle('disabled',
                                                         !e.value.value);
      $('auto-repeat-delay-range').disabled =
          $('auto-repeat-interval-range').disabled = !e.value.value;
      return false;
    },

    /**
     * Handles auto-repeat delay pref change and stops the event from
     * propagating.
     * @param {Event} e Change event.
     * @return {boolean} Whether the event has finished being handled.
     * @private
     */
    handleAutoRepeatDelayPrefChange_: function(e) {
      this.updateSliderFromValue_('auto-repeat-delay-range',
                                  e.value.value,
                                  AUTO_REPEAT_DELAYS);
      return true;
    },

    /**
     * Handles auto-repeat interval pref change and stops the event from
     * propagating.
     * @param {Event} e Change event.
     * @return {boolean} Whether the event has finished being handled.
     * @private
     */
    handleAutoRepeatIntervalPrefChange_: function(e) {
      this.updateSliderFromValue_('auto-repeat-interval-range',
                                  e.value.value,
                                  AUTO_REPEAT_INTERVALS);
      return true;
    },

    /**
     * Show/hide the caps lock remapping section.
     * @private
     */
    showCapsLockOptions_: function(show) {
      $('caps-lock-remapping-section').hidden = !show;
    },

    /**
     * Show/hide the diamond key remapping section.
     * @private
     */
    showDiamondKeyOptions_: function(show) {
      $('diamond-key-remapping-section').hidden = !show;
    },

    /**
     * Sets the slider's value to the number in |values| that is closest to
     * |value|.
     * @param {string} id The slider's ID.
     * @param {number} value The value to find.
     * @param {!Array.<number>} values The array to search.
     * @private
     */
    updateSliderFromValue_: function(id, value, values) {
      var index = values.indexOf(value);
      if (index == -1) {
        var closestValue = Infinity;
        for (var i = 0; i < values.length; i++) {
          if (Math.abs(values[i] - value) <
              Math.abs(closestValue - value)) {
            closestValue = values[i];
            index = i;
          }
        }

        assert(index != -1,
               'Failed to update ' + id + ' from pref with value ' + value);
      }

      $(id).value = index;
    },
  };

  // Forward public APIs to private implementations.
  cr.makePublic(KeyboardOverlay, [
    'showCapsLockOptions',
    'showDiamondKeyOptions',
  ]);

  // Export
  return {
    KeyboardOverlay: KeyboardOverlay
  };
});
