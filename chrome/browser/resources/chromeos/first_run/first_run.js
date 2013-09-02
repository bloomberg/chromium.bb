// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview First run UI.
 */

<include src="step.js">

cr.define('cr.FirstRun', function() {
  return {
    // SVG element representing UI background.
    background_: null,

    // Mask element describing transparent "holes" in background.
    mask_: null,

    // Pattern used for creating new holes.
    hole_pattern_: null,

    // Dictionary keeping all available tutorial steps by their names.
    steps_: {},

    // Element representing step currently shown for user.
    currentStep_: null,

    /**
     * Initializes internal structures and preparing steps.
     */
    initialize: function() {
      disableTextSelectAndDrag();
      this.background_ = $('background');
      this.mask_ = $('mask');
      this.hole_pattern_ = $('hole-pattern');
      var stepElements = document.getElementsByClassName('step');
      for (var i = 0; i < stepElements.length; ++i) {
        var step = stepElements[i];
        cr.FirstRun.Step.decorate(step);
        this.steps_[step.getName()] = step;
      }
      chrome.send('initialized');
    },

    /**
     * Adds transparent hole to background.
     * @param {number} x X coordinate of top-left corner of hole.
     * @param {number} y Y coordinate of top-left corner of hole.
     * @param {number} widht Width of hole.
     * @param {number} height Height of hole.
     */
    addHole: function(x, y, width, height) {
      var hole = this.hole_pattern_.cloneNode();
      hole.removeAttribute('id');
      hole.setAttribute('x', x);
      hole.setAttribute('y', y);
      hole.setAttribute('width', width);
      hole.setAttribute('height', height);
      this.mask_.appendChild(hole);
    },

    /**
     * Removes all holes previously added by |addHole|.
     */
    removeHoles: function() {
      var holes = this.mask_.getElementsByClassName('hole');
      // Removing nodes modifies |holes|, that's why we run reverse cycle.
      for (var i = holes.length - 1; i >= 0; --i) {
        this.mask_.removeChild(holes[i]);
      }
    },

    /**
     * Shows step with given name in given position.
     * @param {string} name Name of step.
     * @param {object} position Optional parameter with optional fields |top|,
     *     |right|, |bottom|, |left| used for step positioning.
     */
    showStep: function(name, position) {
      if (!this.steps_.hasOwnProperty(name))
        throw Error('Step "' + name + '" not found.');
      if (this.currentStep_) {
        this.currentStep_.style.setProperty('display', 'none');
      }
      var step = this.steps_[name];
      var stepStyle = step.style;
      if (position) {
        ['top', 'right', 'bottom', 'left'].forEach(function(property) {
          if (position.hasOwnProperty(property))
            stepStyle.setProperty(property, position[property] + 'px');
        });
      }
      stepStyle.setProperty('display', 'inline-block');
      this.currentStep_ = step;
    }
  };
});

/**
 * Initializes UI.
 */
window.onload = function() {
  cr.FirstRun.initialize();
};

