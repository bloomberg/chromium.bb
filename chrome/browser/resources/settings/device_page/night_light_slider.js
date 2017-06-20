// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * night-light-slider is used to set the custom automatic schedule of the
 * Night Light feature, so that users can set their desired start and end
 * times.
 */

/** @const */ var HOURS_PER_DAY = 24;
/** @const */ var MIN_KNOBS_DISTANCE_MINUTES = 30;
/** @const */ var OFFSET_MINUTES_6PM = 18 * 60;
/** @const */ var TOTAL_MINUTES_PER_DAY = 24 * 60;

Polymer({
  is: 'night-light-slider',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
    Polymer.IronA11yKeysBehavior,
  ],

  properties: {
    /**
     * The object currently being dragged. Either the start or end knobs.
     * @type {?Object}
     * @private
     */
    dragObject_: {
      type: Object,
      value: null,
    },

    /**
     * The start knob time as a string to be shown on the start label bubble.
     * @private
     */
    startTime_: {
      type: String,
    },

    /**
     * The end knob time as a string to be shown on the end label bubble.
     * @private
     */
    endTime_: {
      type: String,
    },
  },

  observers: [
    'customTimesChanged_(prefs.ash.night_light.custom_start_time.*, ' +
        'prefs.ash.night_light.custom_end_time.*)',
  ],

  keyBindings: {
    'left': 'onLeftKey_',
    'right': 'onRightKey_',
  },

  ready: function() {
    // Build the legend markers.
    var markersContainer = this.$.markersContainer;
    var width = markersContainer.offsetWidth;
    for (var i = 0; i <= HOURS_PER_DAY; ++i) {
      var marker = document.createElement('div');
      marker.className = 'markers';
      markersContainer.appendChild(marker);
      marker.style.left = (i * 100 / HOURS_PER_DAY) + '%';
    }
    this.async(function() {
      // Read the initial prefs values and refresh the slider.
      this.customTimesChanged_();
    });
  },

  /**
   * Expands or un-expands the knob being dragged along with its corresponding
   * label bubble.
   * @param {boolean} expand True to expand, and false to un-expand.
   * @private
   */
  setExpanded_: function(expand) {
    var knob = this.$.startKnob;
    var label = this.$.startLabel;
    if (this.dragObject_ == this.$.endKnob) {
      knob = this.$.endKnob;
      label = this.$.endLabel;
    }

    knob.classList.toggle('expanded-knob', expand);
    label.classList.toggle('expanded-knob', expand);
  },

  /**
   * If one of the two knobs is focused, this function blurs it.
   * @private
   */
  blurAnyFocusedKnob_: function() {
    var activeElement = this.shadowRoot.activeElement;
    if (activeElement == this.$.startKnob || activeElement == this.$.endKnob)
      activeElement.blur();
  },

  /**
   * Start dragging the target knob.
   * @private
   */
  startDrag_: function(event) {
    event.preventDefault();
    this.dragObject_ = event.target;
    this.setExpanded_(true);

    // Focus is only given to the knobs by means of keyboard tab navigations.
    // When we start dragging, we don't want to see any focus halos around any
    // knob.
    this.blurAnyFocusedKnob_();

    // However, our night-light-slider element must get the focus.
    this.focus();
  },

  /**
   * Continues dragging the selected knob if any.
   * @private
   */
  continueDrag_: function(event) {
    if (!this.dragObject_)
      return;

    event.stopPropagation();
    switch (event.detail.state) {
      case 'start':
        this.startDrag_(event);
        break;
      case 'track':
        this.doKnobTracking_(event);
        break;
      case 'end':
        this.endDrag_(event);
        break;
    }
  },

  /**
   * Updates the knob's corresponding pref value in response to dragging, which
   * will in turn update the location of the knob and its corresponding label
   * bubble and its text contents.
   * @private
   */
  doKnobTracking_: function(event) {
    var deltaRatio = Math.abs(event.detail.ddx) / this.$.sliderBar.offsetWidth;
    var deltaMinutes = Math.floor(deltaRatio * TOTAL_MINUTES_PER_DAY);
    if (deltaMinutes <= 0)
      return;

    var knobPref = this.dragObject_ == this.$.startKnob ?
        'ash.night_light.custom_start_time' :
        'ash.night_light.custom_end_time';

    if (event.detail.ddx > 0) {
      // Increment the knob's pref by the amount of deltaMinutes.
      this.incrementPref_(knobPref, deltaMinutes);
    } else {
      // Decrement the knob's pref by the amount of deltaMinutes.
      this.decrementPref_(knobPref, deltaMinutes);
    }
  },

  /**
   * Ends the dragging.
   * @private
   */
  endDrag_: function(event) {
    event.preventDefault();
    this.setExpanded_(false);
    this.dragObject_ = null;
  },

  /**
   * Gets the given knob's offset ratio with respect to its parent element
   * (which is the slider bar).
   * @param {HTMLDivElement} knob Either one of the two knobs.
   * @return {number}
   * @private
   */
  getKnobRatio_: function(knob) {
    return parseFloat(knob.style.left) / this.$.sliderBar.offsetWidth;
  },

  /**
   * Converts the time of day, given as |hour| and |minutes|, to its language-
   * sensitive time string representation.
   * @param {number} hour The hour of the day (0 - 23).
   * @param {number} minutes The minutes of the hour (0 - 59).
   * @return {string}
   * @private
   */
  getLocaleTimeString_: function(hour, minutes) {
    var d = new Date();
    d.setHours(hour);
    d.setMinutes(minutes);
    d.setSeconds(0);
    d.setMilliseconds(0);

    return d.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'});
  },

  /**
   * Converts the |offsetMinutes| value (which the number of minutes since
   * 00:00) to its language-sensitive time string representation.
   * @param {number} offsetMinutes The time of day represented as the number of
   * minutes from 00:00.
   * @return {string}
   * @private
   */
  offsetMinutesToTimeString_: function(offsetMinutes) {
    var hour = Math.floor(offsetMinutes / 60);
    var minute = Math.floor(offsetMinutes % 60);

    return this.getLocaleTimeString_(hour, minute);
  },

  /**
   * Handles changes in the start and end times prefs.
   * @private
   */
  customTimesChanged_: function() {
    var startOffsetMinutes = /** @type {number} */ (
        this.getPref('ash.night_light.custom_start_time').value);
    this.startTime_ = this.offsetMinutesToTimeString_(startOffsetMinutes);
    this.updateKnobLeft_(this.$.startKnob, startOffsetMinutes);
    var endOffsetMinutes = /** @type {number} */ (
        this.getPref('ash.night_light.custom_end_time').value);
    this.endTime_ = this.offsetMinutesToTimeString_(endOffsetMinutes);
    this.updateKnobLeft_(this.$.endKnob, endOffsetMinutes);
    this.refresh_();
  },

  /**
   * Updates the absolute left coordinate of the given |knob| based on the time
   * it represents given as an |offsetMinutes| value.
   * @param {HTMLDivElement} knob
   * @param {number} offsetMinutes
   * @private
   */
  updateKnobLeft_: function(knob, offsetMinutes) {
    var offsetAfter6pm =
        (offsetMinutes + TOTAL_MINUTES_PER_DAY - OFFSET_MINUTES_6PM) %
        TOTAL_MINUTES_PER_DAY;
    var ratio = offsetAfter6pm / TOTAL_MINUTES_PER_DAY;

    if (ratio == 0) {
      // If the ratio is 0, then there are two possibilities:
      // - The knob time is 6:00 PM on the left side of the slider.
      // - The knob time is 6:00 PM on the right side of the slider.
      // We need to check the current knob offset ratio to determine which case
      // it is.
      var currentKnobRatio = this.getKnobRatio_(knob);
      ratio = currentKnobRatio > 0.5 ? 1.0 : 0.0;
    }

    knob.style.left = (ratio * this.$.sliderBar.offsetWidth) + 'px';
  },

  /**
   * Refreshes elements of the slider other than the knobs (the label bubbles,
   * and the progress bar).
   * @private
   */
  refresh_: function() {
    var endKnob = this.$.endKnob;
    var startKnob = this.$.startKnob;
    var startProgress = this.$.startProgress;
    var endProgress = this.$.endProgress;
    var startLabel = this.$.startLabel;
    var endLabel = this.$.endLabel;

    // The label bubbles have the same left coordinates as their corresponding
    // knobs.
    startLabel.style.left = startKnob.style.left;
    endLabel.style.left = endKnob.style.left;

    // The end progress bar starts from either the start knob or the start of
    // the slider (whichever is to its left) and ends at the end knob.
    var endProgressLeft = startKnob.offsetLeft >= endKnob.offsetLeft ?
        '0px' :
        startKnob.style.left;
    endProgress.style.left = endProgressLeft;
    endProgress.style.width =
        (parseFloat(endKnob.style.left) - parseFloat(endProgressLeft)) + 'px';

    // The start progress bar starts at the start knob, and ends at either the
    // end knob or the end of the slider (whichever is to its right).
    var startProgressRight = endKnob.offsetLeft < startKnob.offsetLeft ?
        this.$.sliderBar.offsetWidth :
        endKnob.style.left;
    startProgress.style.left = startKnob.style.left;
    startProgress.style.width =
        (parseFloat(startProgressRight) - parseFloat(startKnob.style.left)) +
        'px';

    this.fixLabelsOverlapIfAny_();
  },

  /**
   * If the label bubbles overlap, this function fixes them by moving the end
   * label up a little.
   * @private
   */
  fixLabelsOverlapIfAny_: function() {
    var startLabel = this.$.startLabel;
    var endLabel = this.$.endLabel;
    var distance = Math.abs(
        parseFloat(startLabel.style.left) - parseFloat(endLabel.style.left));
    if (distance <= startLabel.offsetWidth) {
      // Shift the end label up so that it doesn't overlap with the start label.
      endLabel.classList.add('end-label-overlap');
    } else {
      endLabel.classList.remove('end-label-overlap');
    }
  },

  /**
   * Given the |prefPath| that corresponds to one knob time, it gets the value
   * of the pref that corresponds to the other knob.
   * @param {string} prefPath
   * @return {number}
   * @private
   */
  getOtherKnobPrefValue_: function(prefPath) {
    if (prefPath == 'ash.night_light.custom_start_time') {
      return /** @type {number} */ (
          this.getPref('ash.night_light.custom_end_time').value);
    }

    return /** @type {number} */ (
        this.getPref('ash.night_light.custom_start_time').value);
  },

  /**
   * Increments the value of the pref whose path is given by |prefPath| by the
   * amount given in |increment|.
   * @param {string} prefPath
   * @param {number} increment
   * @private
   */
  incrementPref_: function(prefPath, increment) {
    var value = this.getPref(prefPath).value + increment;

    var otherValue = this.getOtherKnobPrefValue_(prefPath);
    if (otherValue > value &&
        ((otherValue - value) < MIN_KNOBS_DISTANCE_MINUTES)) {
      // We are incrementing the minutes offset moving towards the other knob.
      // We have a minimum 30 minutes overlap threshold. Move this knob to the
      // other side of the other knob.
      //
      // Was:
      // ------ (+) --- 29 MIN --- + ------->>
      //
      // Now:
      // ------ + --- 30 MIN --- (+) ------->>
      //
      // (+) ==> Knob being moved.
      value = otherValue + MIN_KNOBS_DISTANCE_MINUTES;
    }

    // The knobs are allowed to wrap around.
    this.setPrefValue(prefPath, (value % TOTAL_MINUTES_PER_DAY));
  },

  /**
   * Decrements the value of the pref whose path is given by |prefPath| by the
   * amount given in |decrement|.
   * @param {string} prefPath
   * @param {number} decrement
   * @private
   */
  decrementPref_: function(prefPath, decrement) {
    var value =
        /** @type {number} */ (this.getPref(prefPath).value) - decrement;

    var otherValue = this.getOtherKnobPrefValue_(prefPath);
    if (value > otherValue &&
        ((value - otherValue) < MIN_KNOBS_DISTANCE_MINUTES)) {
      // We are decrementing the minutes offset moving towards the other knob.
      // We have a minimum 30 minutes overlap threshold. Move this knob to the
      // other side of the other knob.
      //
      // Was:
      // <<------ + --- 29 MIN --- (+) -------
      //
      // Now:
      // <<------ (+) --- 30 MIN --- + ------
      //
      // (+) ==> Knob being moved.
      value = otherValue - MIN_KNOBS_DISTANCE_MINUTES;
    }

    // The knobs are allowed to wrap around.
    if (value < 0)
      value += TOTAL_MINUTES_PER_DAY;

    this.setPrefValue(prefPath, Math.abs(value) % TOTAL_MINUTES_PER_DAY);
  },

  /**
   * Gets the pref path of the currently focused knob. Returns null if no knob
   * is currently focused.
   * @return {?string}
   * @private
   */
  getFocusedKnobPrefPathIfAny_: function() {
    var focusedElement = this.shadowRoot.activeElement;
    if (focusedElement == this.$.startKnob)
      return 'ash.night_light.custom_start_time';

    if (focusedElement == this.$.endKnob)
      return 'ash.night_light.custom_end_time';

    return null;
  },

  /**
   * Handles the 'left' key event.
   * @private
   */
  onLeftKey_: function(e) {
    e.preventDefault();
    var knobPref = this.getFocusedKnobPrefPathIfAny_();
    if (!knobPref)
      return;

    this.decrementPref_(knobPref, 1);
  },

  /**
   * Handles the 'right' key event.
   * @private
   */
  onRightKey_: function(e) {
    e.preventDefault();
    var knobPref = this.getFocusedKnobPrefPathIfAny_();
    if (!knobPref)
      return;

    this.incrementPref_(knobPref, 1);
  },
});