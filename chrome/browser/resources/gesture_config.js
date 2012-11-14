// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Redefine '$' here rather than including 'cr.js', since this is
// the only function needed.  This allows this file to be loaded
// in a browser directly for layout and some testing purposes.
var $ = function(id) { return document.getElementById(id); };

/**
 * WebUI for configuring gesture.* preference values used by
 * Chrome's gesture recognition system.
 */
var gesture_config = (function() {
  'use strict';

  /** Common prefix of gesture preferences. **/
  /** @const */ var GESTURE_PREFIX = 'gesture.';

  /** List of fields used to dynamically build form. **/
  var FIELDS = [
    {
      key: 'long_press_time_in_seconds',
      label: 'Long Press Time',
      units: 'seconds'
    },
    {
      key: 'semi_long_press_time_in_seconds',
      label: 'Semi Long Press Time',
      units: 'seconds'
    },
    {
      key: 'max_seconds_between_double_click',
      label: 'Maximum Double Click Interval',
      units: 'seconds',
      step: 0.1
    },
    {
      key: 'max_separation_for_gesture_touches_in_pixels',
      label: 'Maximum Separation for Gesture Touches',
      units: 'pixels'
    },
    {
      key: 'max_swipe_deviation_ratio',
      label: 'Maximum Swipe Deviation',
      units: ''
    },
    {
      key: 'max_touch_down_duration_in_seconds_for_click',
      label: 'Maximum Touch-Down Duration for Click',
      units: 'seconds',
      step: 0.1
    },
    {
      key: 'max_touch_move_in_pixels_for_click',
      label: 'Maximum Touch-Move for Click',
      units: 'pixels'
    },
    {
      key: 'max_distance_between_taps_for_double_tap',
      label: 'Maximum Distance between two taps for Double Tap',
      units: 'pixels'
    },
    {
      key: 'min_distance_for_pinch_scroll_in_pixels',
      label: 'Minimum Distance for Pinch Scroll',
      units: 'pixels'
    },
    {
      key: 'min_flick_speed_squared',
      label: 'Minimum Flick Speed Squared',
      units: '(pixels/sec.)<sup>2</sup>'
    },
    {
      key: 'min_pinch_update_distance_in_pixels',
      label: 'Minimum Pinch Update Distance',
      units: 'pixels'
    },
    {
      key: 'min_rail_break_velocity',
      label: 'Minimum Rail-Break Velocity',
      units: 'pixels/sec.'
    },
    {
      key: 'min_scroll_delta_squared',
      label: 'Minimum Scroll Delta Squared',
      units: ''
    },
    {
      key: 'min_swipe_speed',
      label: 'Minimum Swipe Speed',
      units: 'pixels/sec.'
    },
    {
      key: 'min_touch_down_duration_in_seconds_for_click',
      label: 'Minimum Touch-Down Duration for Click',
      units: 'seconds',
      step: 0.01
    },
    {
      key: 'points_buffered_for_velocity',
      label: 'Points Buffered for Velocity',
      units: '',
      step: 1
    },
    {
      key: 'rail_break_proportion',
      label: 'Rail-Break Proportion',
      units: '%'
    },
    {
      key: 'rail_start_proportion',
      label: 'Rail-Start Proportion',
      units: '%'
    },
    {
      key: 'fling_acceleration_curve_coefficient_0',
      label: 'Touchscreen Fling Acceleration',
      units: 'x<sup>3</sup>'
    },
    {
      key: 'fling_acceleration_curve_coefficient_1',
      label: '+',
      units: 'x<sup>2</sup>'
    },
    {
      key: 'fling_acceleration_curve_coefficient_2',
      label: '+',
      units: 'x<sup>1</sup>'
    },
    {
      key: 'fling_acceleration_curve_coefficient_3',
      label: '+',
      units: 'x<sup>0</sup>'
    }
  ];

  /**
   * Dynamically builds web-form based on FIELDS list.
   * @return {string} The form's HTML.
   */
  function buildForm() {
    var buf = [];

    for (var i = 0; i < FIELDS.length; i++) {
      var field = FIELDS[i];

      var row = $('gesture-form-row').cloneNode(true);
      var label = row.querySelector('.row-label');
      var input = row.querySelector('.row-input');
      var units = row.querySelector('.row-units');

      row.id = '';
      label.setAttribute('for', field.key);
      label.textContent = field.label;
      input.id = field.key;
      input.min = field.min || 0;

      if (field.max) input.max = field.max;
      if (field.step) input.step = field.step;

      $('gesture-form').appendChild(row);
      if (field.units)
        units.innerHTML = field.units;
    }
  }

  /**
   * Initialize the form by adding 'onChange' listeners to all fields.
   */
  function initForm() {
    for (var i = 0; i < FIELDS.length; i++) {
      var field = FIELDS[i];
      $(field.key).onchange = (function(key) {
        setPreferenceValue(key, $(key).value);
      }).bind(null, field.key);
    }
  }

  /**
   * Request a preference setting's value.
   * This method is asynchronous; the result is provided by a call to
   * getPreferenceValueResult.
   * @param {string} prefName The name of the preference value being requested.
   */
  function getPreferenceValue(prefName) {
    chrome.send('getPreferenceValue', [GESTURE_PREFIX + prefName]);
  }

  /**
   * Handle callback from call to getPreferenceValue.
   * @param {string} prefName The name of the requested preference value.
   * @param {value} value The current value associated with prefName.
   */
  function getPreferenceValueResult(prefName, value) {
    prefName = prefName.substring(prefName.indexOf('.') + 1);
    $(prefName).value = value;
  }

  /**
   * Set a preference setting's value.
   * @param {string} prefName The name of the preference value being set.
   * @param {value} value The value to be associated with prefName.
   */
  function setPreferenceValue(prefName, value) {
    chrome.send(
        'setPreferenceValue',
        [GESTURE_PREFIX + prefName, parseFloat(value)]);
  }

  /**
   * Reset a preference to its default value and get that callback
   * to getPreferenceValueResult with the new value of the preference.
   * @param {string} prefName The name of the requested preference.
   */
  function resetPreferenceValue(prefName) {
    chrome.send('resetPreferenceValue', [GESTURE_PREFIX + prefName]);
  }

  /**
   * Handle processing of "Reset" button.
   * Causes off form values to be updated based on current preference values.
   */
  function onReset() {
    for (var i = 0; i < FIELDS.length; i++) {
      var field = FIELDS[i];
      resetPreferenceValue(field.key);
    }
    return false;
  }

  function loadForm() {
    for (var i = 0; i < FIELDS.length; i++)
      getPreferenceValue(FIELDS[i].key);
  }

  /**
   * Build and initialize the gesture configuration form.
   */
  function initialize() {
    buildForm();
    loadForm();
    initForm();

    $('reset-button').onclick = onReset.bind(this);
  }

  return {
    initialize: initialize,
    getPreferenceValueResult: getPreferenceValueResult
  };
})();

document.addEventListener('DOMContentLoaded', gesture_config.initialize);
