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

    // Container for background.
    backgroundContainer_: null,

    // Mask element describing transparent "holes" in background.
    mask_: null,

    // Pattern used for creating rectangular holes.
    rectangularHolePattern_: null,

    // Pattern used for creating round holes.
    roundHolePattern_: null,

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
      this.backgroundContainer_ = $('background-container');
      this.mask_ = $('mask');
      this.rectangularHolePattern_ = $('rectangular-hole-pattern');
      this.rectangularHolePattern_.removeAttribute('id');
      this.roundHolePattern_ = $('round-hole-pattern');
      this.roundHolePattern_.removeAttribute('id');
      var stepElements = document.getElementsByClassName('step');
      for (var i = 0; i < stepElements.length; ++i) {
        var step = stepElements[i];
        cr.FirstRun.DecorateStep(step);
        this.steps_[step.getName()] = step;
      }
      chrome.send('initialized');
    },

    /**
     * Adds transparent rectangular hole to background.
     * @param {number} x X coordinate of top-left corner of hole.
     * @param {number} y Y coordinate of top-left corner of hole.
     * @param {number} widht Width of hole.
     * @param {number} height Height of hole.
     */
    addRectangularHole: function(x, y, width, height) {
      var hole = this.rectangularHolePattern_.cloneNode();
      hole.setAttribute('x', x);
      hole.setAttribute('y', y);
      hole.setAttribute('width', width);
      hole.setAttribute('height', height);
      this.mask_.appendChild(hole);
    },

    /**
     * Adds transparent round hole to background.
     * @param {number} x X coordinate of circle center.
     * @param {number} y Y coordinate of circle center.
     * @param {number} radius Radius of circle.
     */
    addRoundHole: function(x, y, radius) {
      var hole = this.roundHolePattern_.cloneNode();
      hole.setAttribute('cx', x);
      hole.setAttribute('cy', y);
      hole.setAttribute('r', radius);
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
     * @param {Array} pointWithOffset Optional parameter for positioning
     *     bubble. Contains [x, y, offset], where (x, y) - point to which bubble
     *     points, offset - distance between arrow and point.
     */
    showStep: function(name, position, pointWithOffset) {
      if (!this.steps_.hasOwnProperty(name))
        throw Error('Step "' + name + '" not found.');
      if (this.currentStep_)
        this.currentStep_.hide();
      var step = this.steps_[name];
      if (position)
        step.setPosition(position);
      if (pointWithOffset)
        step.setPointsTo(pointWithOffset.slice(0, 2), pointWithOffset[2]);
      step.show();
      this.currentStep_ = step;
    },

    /**
     * Sets visibility of the background.
     * @param {boolean} visibility Whether background should be visible.
     */
    setBackgroundVisible: function(visible) {
      this.backgroundContainer_.hidden = !visible;
    }
  };
});

/**
 * Initializes UI.
 */
window.onload = function() {
  cr.FirstRun.initialize();
};

