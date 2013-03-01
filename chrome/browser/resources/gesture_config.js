// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Redefine '$' here rather than including 'cr.js', since this is
// the only function needed.  This allows this file to be loaded
// in a browser directly for layout and some testing purposes.
var $ = function(id) { return document.getElementById(id); };

/**
 * A generic WebUI for configuring preference values used by Chrome's gesture
 * recognition systems.
 * @param {string} title The user-visible title to display for the configuration
 *    section.
 * @param {string} prefix The prefix for the configuration fields.
 * @param {!Object} fields An array of fields that contain the name of the pref
 *    and user-visible labels.
 */
function GeneralConfig(title, prefix, fields) {
  this.title = title;
  this.prefix = prefix;
  this.fields = fields;
}

GeneralConfig.prototype = {
  /**
   * Sets up the form for configuring all the preference values.
   */
  buildAll: function() {
    this.buildForm();
    this.loadForm();
    this.initForm();
  },

  /**
   * Dynamically builds web-form based on the list of preferences.
   */
  buildForm: function() {
    var buf = [];

    var section = $('section-template').cloneNode(true);
    section.removeAttribute('id');
    var title = section.querySelector('.section-title');
    title.textContent = this.title;

    for (var i = 0; i < this.fields.length; i++) {
      var field = this.fields[i];

      var row = $('section-row-template').cloneNode(true);
      row.removeAttribute('id');

      var label = row.querySelector('.row-label');
      var input = row.querySelector('.input');
      var units = row.querySelector('.row-units');

      label.setAttribute('for', field.key);
      label.textContent = field.label;
      input.id = field.key;
      input.min = field.min || 0;

      if (field.max) input.max = field.max;
      if (field.step) input.step = field.step;

      if (field.units)
        units.innerHTML = field.units;

      section.querySelector('.section-properties').appendChild(row);
    }
    $('gesture-form').appendChild(section);
  },

  /**
   * Initializes the form by adding 'onChange' listeners to all fields.
   */
  initForm: function() {
    for (var i = 0; i < this.fields.length; i++) {
      var field = this.fields[i];
      var config = this;
      $(field.key).onchange = (function(key) {
        config.setPreferenceValue(key, $(key).value);
      }).bind(null, field.key);
    }
  },

  /**
   * Requests preference values for all the relevant fields.
   */
  loadForm: function() {
    for (var i = 0; i < this.fields.length; i++)
      this.getPreferenceValue(this.fields[i].key);
  },

  /**
   * Handles processing of "Reset" button.
   * Causes all form values to be updated based on current preference values.
   * @return {bool} Returns false.
   */
  onReset: function() {
    for (var i = 0; i < this.fields.length; i++) {
      var field = this.fields[i];
      this.resetPreferenceValue(field.key);
    }
    return false;
  },

  /**
   * Requests a preference setting's value.
   * This method is asynchronous; the result is provided by a call to
   * getPreferenceValueResult.
   * @param {string} prefName The name of the preference value being requested.
   */
  getPreferenceValue: function(prefName) {
    chrome.send('getPreferenceValue', [this.prefix + prefName]);
  },

  /**
   * Sets a preference setting's value.
   * @param {string} prefName The name of the preference value being set.
   * @param {value} value The value to be associated with prefName.
   */
  setPreferenceValue: function(prefName, value) {
    chrome.send('setPreferenceValue',
        [this.prefix + prefName, parseFloat(value)]);
  },

  /**
   * Resets a preference to its default value and get that callback
   * to getPreferenceValueResult with the new value of the preference.
   * @param {string} prefName The name of the requested preference.
   */
  resetPreferenceValue: function(prefName) {
    chrome.send('resetPreferenceValue', [this.prefix + prefName]);
  }
};

/**
 * Returns a GeneralConfig for configuring gestures.* preferences.
 * @return {object} A GeneralConfig object.
 */
function GestureConfig() {
  /** The title of the section for the gesture preferences. **/
  /** @const */ var GESTURE_TITLE = 'Gesture Configuration';

  /** Common prefix of gesture preferences. **/
  /** @const */ var GESTURE_PREFIX = 'gesture.';

  /** List of fields used to dynamically build form. **/
  var GESTURE_FIELDS = [
    {
      key: 'fling_max_cancel_to_down_time_in_ms',
      label: 'Maximum Cancel to Down Time for Tap Suppression',
      units: 'milliseconds',
    },
    {
      key: 'fling_max_tap_gap_time_in_ms',
      label: 'Maximum Tap Gap Time for Tap Suppression',
      units: 'milliseconds',
    },
    {
      key: 'long_press_time_in_seconds',
      label: 'Long Press Time',
      units: 'seconds'
    },
    {
      key: 'semi_long_press_time_in_seconds',
      label: 'Semi Long Press Time',
      units: 'seconds',
      step: 0.1
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
    },
    {
      key: 'fling_velocity_cap',
      label: 'Touchscreen Fling Velocity Cap',
      units: 'pixels / second'
    },
    {
      key: 'tab_scrub_activation_delay_in_ms',
      label: 'Tab scrub auto activation delay, (-1 for never)',
      units: 'milliseconds'
    }
  ];

  return new GeneralConfig(GESTURE_TITLE, GESTURE_PREFIX, GESTURE_FIELDS);
}

/**
 * Returns a GeneralConfig for configuring overscroll.* preferences.
 * @return {object} A GeneralConfig object.
 */
function OverscrollConfig() {
  /** @const */ var OVERSCROLL_TITLE = 'Overscroll Configuration';

  /** @const */ var OVERSCROLL_PREFIX = 'overscroll.';

  var OVERSCROLL_FIELDS = [
    {
      key: 'horizontal_threshold_complete',
      label: 'Complete when overscrolled (horizontal)',
      units: '%'
    },
    {
      key: 'vertical_threshold_complete',
      label: 'Complete when overscrolled (vertical)',
      units: '%'
    },
    {
      key: 'minimum_threshold_start',
      label: 'Start overscroll gesture after scrolling',
      units: 'pixels'
    },
    {
      key: 'horizontal_resist_threshold',
      label: 'Start resisting overscroll after (horizontal)',
      units: 'pixels'
    },
    {
      key: 'vertical_resist_threshold',
      label: 'Start resisting overscroll after (vertical)',
      units: 'pixels'
    },
  ];

  return new GeneralConfig(OVERSCROLL_TITLE,
                           OVERSCROLL_PREFIX,
                           OVERSCROLL_FIELDS);
}

/**
 * Returns a GeneralConfig for configuring workspace_cycler.* preferences.
 * @return {object} A GeneralConfig object.
 */
function WorkspaceCyclerConfig() {
  /** @const */ var WORKSPACE_CYCLER_TITLE = 'Workspace Cycler Configuration';

  /** @const */ var WORKSPACE_CYCLER_PREFIX = 'workspace_cycler.';

  var WORKSPACE_CYCLER_FIELDS = [
    {
      key: 'selected_scale',
      label: 'Scale of the selected workspace',
      units: '%'
    },
    {
      key: 'min_scale',
      label: 'Minimum workspace scale (scale of deepest workspace)',
      units: '%'
    },
    {
      key: 'max_scale',
      label: 'Maximimum workspace scale (scale of shallowest workspace)',
      units: '%'
    },
    {
      key: 'min_brightness',
      label: 'Minimum workspace brightness (deepest & shallowest workspace)',
      units: '%'
    },
    {
      key: 'background_opacity',
      label: 'Desktop background opacity when cycling through workspaces',
      units: '%'
    },
    {
      key: 'distance_to_initiate_cycling',
      label: 'Vertical distance to scroll to initiate cycling',
      units: 'pixels'
    },
    {
      key: 'scroll_distance_to_cycle_to_next_workspace',
      label: 'Vertical distance to scroll to cycle to the next workspace',
      units: 'pixels'
    },
    { key: 'cycler_step_animation_duration_ratio',
      label: 'Cycler step animation duration ratio',
      units: 'ms / pixels vertical scroll'
    },
    { key: 'start_cycler_animation_duration',
      label: 'Duration of the animations to start cycling',
      units: 'ms'
    },
    { key: 'stop_cycler_animation_duration',
      label: 'Duration of the animations to stop cycling',
      units: 'ms'
    }
  ];

  return new GeneralConfig(WORKSPACE_CYCLER_TITLE,
                           WORKSPACE_CYCLER_PREFIX,
                           WORKSPACE_CYCLER_FIELDS);
}

/**
 * Returns a GeneralConfig for configuring flingcurve.* preferences.
 * @return {object} A GeneralConfig object.
 */
function FlingConfig() {
  /** @const */ var FLING_TITLE = 'Fling Configuration';

  /** @const */ var FLING_PREFIX = 'flingcurve.';

  var FLING_FIELDS = [
    {
      key: 'touchscreen_alpha',
      label: 'Touchscreen fling deacceleration coefficients',
      units: 'alpha'
    },
    {
      key: 'touchscreen_beta',
      label: '',
      units: 'beta'
    },
    {
      key: 'touchscreen_gamma',
      label: '',
      units: 'gamma'
    },
    {
      key: 'touchpad_alpha',
      label: 'Touchpad fling deacceleration coefficients',
      units: 'alpha'
    },
    {
      key: 'touchpad_beta',
      label: '',
      units: 'beta'
    },
    {
      key: 'touchpad_gamma',
      label: '',
      units: 'gamma'
    },
  ];

  return new GeneralConfig(FLING_TITLE, FLING_PREFIX, FLING_FIELDS);
}


/**
 * WebUI instance for configuring gesture.* and overscroll.* preference values
 * used by Chrome's gesture recognition system.
 */
var gesture_config = (function() {

  /**
   * Build and initialize the gesture configuration form.
   */
  function initialize() {
    var g = GestureConfig();
    g.buildAll();

    var o = OverscrollConfig();
    o.buildAll();

    var f = FlingConfig();
    f.buildAll();

    var c = WorkspaceCyclerConfig();
    c.buildAll();

    $('reset-button').onclick = function() {
      g.onReset();
      o.onReset();
      f.onReset();
      c.onReset();
    };
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

  return {
    initialize: initialize,
    getPreferenceValueResult: getPreferenceValueResult
  };
})();

document.addEventListener('DOMContentLoaded', gesture_config.initialize);
