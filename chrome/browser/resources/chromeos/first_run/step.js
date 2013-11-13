// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Prototype for first-run tutorial steps.
 */

cr.define('cr.FirstRun', function() {
  var Step = cr.ui.define('div');

  Step.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Name of step.
    name_: null,

    // Button leading to next tutorial step.
    nextButton_: null,

    // Whether screen is shown.
    isShown_: false,

    decorate: function() {
      this.name_ = this.getAttribute('id');
      this.nextButton_ = this.getElementsByClassName('next-button')[0];
      if (!this.nextButton_)
        throw Error('Next button not found.');
      this.nextButton_.addEventListener('click', (function(e) {
        chrome.send('nextButtonClicked', [this.getName()]);
        e.stopPropagation();
      }).bind(this));
      // Make close unfocusable by mouse.
      var topBar = this.getElementsByClassName('topbutton-bar')[0];
      if (topBar) {
        var closeButton = topBar.getElementsByClassName('close-button')[0];
        if (closeButton) {
          closeButton.addEventListener('mousedown', function(event) {
            event.preventDefault();
          });
        }
      }
    },

    /**
     * Returns name of the string.
     */
    getName: function() {
      return this.name_;
    },

    /**
     * Hides the step.
     */
    hide: function() {
      this.style.setProperty('display', 'none');
      this.isShown_ = false;
    },

    /**
     * Shows the step.
     */
    show: function() {
      this.style.setProperty('display', 'inline-block');
      this.isShown_ = true;
    },

    /**
     * Sets position of the step.
     * @param {object} position Parameter with optional fields |top|,
     *     |right|, |bottom|, |left| holding corresponding offsets.
     */
    setPosition: function(position) {
      var style = this.style;
      ['top', 'right', 'bottom', 'left'].forEach(function(property) {
        if (position.hasOwnProperty(property))
          style.setProperty(property, position[property] + 'px');
      });
    }
  };

  var Bubble = cr.ui.define('div');

  // Styles of .step which are used for arrow styling.
  var ARROW_STYLES = [
    'points-up',
    'points-left',
    'points-down',
    'points-right',
    'top',
    'left',
    'bottom',
    'right'
  ];

  var DISTANCE_TO_POINTEE = 10;
  var MINIMAL_SCREEN_OFFSET = 10;
  var ARROW_LENGTH = 6; // Keep synced with .arrow border-width.

  Bubble.prototype = {
    __proto__: Step.prototype,

    // Element displaying arrow.
    arrow_: null,

    // Unit vector directed along the bubble arrow.
    direction_: null,

    /**
     * In addition to base class 'decorate' this method creates arrow and
     * sets some properties related to arrow.
     */
    decorate: function() {
      Step.prototype.decorate.call(this);
      this.arrow_ = document.createElement('div');
      this.arrow_.classList.add('arrow');
      this.appendChild(this.arrow_);
      ARROW_STYLES.forEach(function(style) {
        if (!this.classList.contains(style))
          return;
        // Changing right to left in RTL case.
        if (document.documentElement.getAttribute('dir') == 'rtl') {
          style = style.replace(/right|left/, function(match) {
            return (match == 'right') ? 'left' : 'right';
          });
        }
        this.arrow_.classList.add(style);
      }.bind(this));
      var list = this.arrow_.classList;
      if (list.contains('points-up'))
        this.direction_ = [0, -1];
      else if (list.contains('points-right'))
        this.direction_ = [1, 0];
      else if (list.contains('points-down'))
        this.direction_ = [0, 1];
      else // list.contains('points-left')
        this.direction_ = [-1, 0];
    },

    /**
     * Sets position of bubble in such a maner that bubble's arrow points to
     * given point.
     * @param {Array} point Bubble arrow should point to this point after
     *     positioning. |point| has format [x, y].
     * @param {offset} number Additional offset from |point|.
     */
    setPointsTo: function(point, offset) {
      var shouldShowBefore = !this.isShown_;
      // "Showing" bubble in order to make offset* methods work.
      if (shouldShowBefore) {
        this.style.setProperty('opacity', '0');
        this.show();
      }
      var arrow = [this.arrow_.offsetLeft + this.arrow_.offsetWidth / 2,
                   this.arrow_.offsetTop + this.arrow_.offsetHeight / 2];
      var totalOffset = DISTANCE_TO_POINTEE + offset;
      var left = point[0] - totalOffset * this.direction_[0] - arrow[0];
      var top = point[1] - totalOffset * this.direction_[1] - arrow[1];
      // Force bubble to be inside screen.
      if (this.arrow_.classList.contains('points-up') ||
          this.arrow_.classList.contains('points-down')) {
        left = Math.max(left, MINIMAL_SCREEN_OFFSET);
        left = Math.min(left, document.body.offsetWidth - this.offsetWidth -
            MINIMAL_SCREEN_OFFSET);
      }
      if (this.arrow_.classList.contains('points-left') ||
          this.arrow_.classList.contains('points-right')) {
        top = Math.max(top, MINIMAL_SCREEN_OFFSET);
        top = Math.min(top, document.body.offsetHeight - this.offsetHeight -
            MINIMAL_SCREEN_OFFSET);
      }
      this.style.setProperty('left', left + 'px');
      this.style.setProperty('top', top + 'px');
      if (shouldShowBefore) {
        this.hide();
        this.style.setProperty('opacity', '1');
      }
    },

    /**
     * Sets position of bubble. Overrides Step.setPosition to adjust offsets
     * in case if its direction is the same as arrow's direction.
     * @param {object} position Parameter with optional fields |top|,
     *     |right|, |bottom|, |left| holding corresponding offsets.
     */
    setPosition: function(position) {
      var arrow = this.arrow_;
      // Increasing offset if it's from side where bubble points to.
      [['top', 'points-up'],
       ['right', 'points-right'],
       ['bottom', 'points-down'],
       ['left', 'points-left']].forEach(function(mapping) {
          if (position.hasOwnProperty(mapping[0]) &&
              arrow.classList.contains(mapping[1])) {
            position[mapping[0]] += ARROW_LENGTH + DISTANCE_TO_POINTEE;
          }
        });
      Step.prototype.setPosition.call(this, position);
    }
  };

  var DecorateStep = function(el) {
    if (el.classList.contains('bubble'))
      Bubble.decorate(el);
    else
      Step.decorate(el);
  };

  return {DecorateStep: DecorateStep};
});

