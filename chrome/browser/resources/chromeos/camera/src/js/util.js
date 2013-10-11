// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for utilities.
 */
camera.util = camera.util || {};

/**
 * Sets a class which invokes an animation and calls the callback when the
 * animation is done. The class is released once the animation is finished.
 * If the class name is already set, then calls onCompletion immediately.
 *
 * @param {HTMLElement} classElement Element to be applied the class on.
 * @param {HTMLElement} animationElement Element to be animated.
 * @param {string} className Class name to be added.
 * @param {number} timeout Animation timeout in milliseconds.
 * @param {function()=} opt_onCompletion Completion callback.
 */
camera.util.setAnimationClass = function(
    classElement, animationElement, className, timeout, opt_onCompletion) {
  if (classElement.classList.contains(className)) {
    if (opt_onCompletion)
      opt_onCompletion();
    return;
  }

  classElement.classList.add(className);
  var onAnimationCompleted = function() {
    classElement.classList.remove(className);
    if (opt_onCompletion)
      opt_onCompletion();
  };

  camera.util.waitForAnimationCompletion(
      animationElement, timeout, onAnimationCompleted);
};

/**
 * Waits for animation completion and calls the callback.
 *
 * @param {HTMLElement} animationElement Element to be animated.
 * @param {number} timeout Timeout for completion.
 * @param {function()} onCompletion Completion callback.
 */
camera.util.waitForAnimationCompletion = function(
    animationElement, timeout, onCompletion) {
  var completed = false;
  var onAnimationCompleted = function() {
    if (completed)
      return;
    completed = true;
    animationElement.removeEventListener(onAnimationCompleted);
    onCompletion();
  };
  setTimeout(onAnimationCompleted, timeout);
  animationElement.addEventListener('webkitAnimationEnd', onAnimationCompleted);
};

/**
 * Waits for transition completion and calls the callback.
 *
 * @param {HTMLElement} transitionElement Element to be transitioned.
 * @param {number} timeout Timeout for completion.
 * @param {function()} onCompletion Completion callback.
 */
camera.util.waitForTransitionCompletion = function(
    transitionElement, timeout, onCompletion) {
  var completed = false;
  var onTransitionCompleted = function() {
    if (completed)
      return;
    completed = true;
    transitionElement.removeEventListener(onTransitionCompleted);
    onCompletion();
  };
  setTimeout(onTransitionCompleted, timeout);
  transitionElement.addEventListener(
      'webkitTransitionEnd', onTransitionCompleted);
};

/**
 * Scrolls the parent of the element so the element is visible.
 *
 * @param {HTMLElement} element Element to be visible.
 * @param {camera.util.SmoothScroller} scroller Scroller to be used.
 */
camera.util.ensureVisible = function(element, scroller) {
  var parent = scroller.element;
  var scrollLeft = parent.scrollLeft;
  var scrollTop = parent.scrollTop;
  if (element.offsetTop < parent.scrollTop)
    scrollTop = element.offsetTop - element.offsetHeight * 0.5;
  if (element.offsetTop + element.offsetHeight >
      scrollTop + parent.offsetHeight) {
    scrollTop = element.offsetTop + element.offsetHeight * 1.5 -
        parent.offsetHeight;
  }
  if (element.offsetLeft < parent.scrollLeft)
    scrollLeft = element.offsetLeft - element.offsetWidth * 0.5;
  if (element.offsetLeft + element.offsetWidth >
      scrollLeft + parent.offsetWidth) {
    scrollLeft = element.offsetLeft + element.offsetWidth * 1.5 -
        parent.offsetWidth;
  }
  if (scrollTop != parent.scrollTop ||
      scrollLeft != parent.scrollLeft) {
    scroller.scrollTo(scrollLeft, scrollTop);
  }
};

/**
 * Wraps an effect in style implemented as either CSS3 animation or CSS3
 * transition. The passed closure should invoke the effect.
 * Only the last callback, passed to the latest invoke() call will be called
 * on the transition or the animation end.
 *
 * @param {function(*, function())} closure Closure for invoking the effect.
 * @constructor
 */
camera.util.StyleEffect = function(closure) {
  /**
   * @type {function(*, function()}
   * @private
   */
  this.closure_ = closure;

  /**
   * Callback to be called for the latest invokation.
   * @type {?function()}
   * @private
   */
  this.callback_ = null;

  // End of properties. Seal the object.
  Object.seal(this);
};

/**
 * Invokes the animation and calls the callback on completion. Note, that
 * the callback will not be called if there is another invocation called after.
 *
 * @param {*} state State of the effect to be set
 * @param {function()} callback Completion callback.
 */
camera.util.StyleEffect.prototype.invoke = function(state, callback) {
  this.callback_ = callback;
  this.closure_(state, function() {
    if (!this.callback_)
      return;
    var callback = this.callback_;
    this.callback_ = null;

    // Let the animation neatly finish.
    setTimeout(callback, 0);
  }.bind(this));
};

/**
 * Checks whether the effect is in progress or already finished.
 * @return {boolean} True if active, false otherwise.
 */
camera.util.StyleEffect.prototype.isActive = function() {
  return !!this.callback_;
};

/**
 * Performs smooth scrolling of a scrollable dom element.
 *
 * @param {HTMLElement} element Element to be scerolled.
 * @constructor
 */
camera.util.SmoothScroller = function(element) {
  /**
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * Last value of scrollLeft and scrollTop to detect if a the scroll value
   * has been changed externally while animating.
   * @type {Array.<number>}
   * @private
   */
  this.lastScrollPosition_ = null;

  /**
   * Current, not rounded scroll position when animating.
   * @type {Array.<number>}
   * @private
   */
  this.currentPosition_ = null;

  /**
   * Target position, set to the value passed to scrollTo().
   * @type {Array.<number>}
   * @private
   */
  this.targetPosition_ = null;

  // End of properties. Seal the object.
  Object.seal(this);
};

/**
 * Speed of the scrolling animation. Between 0 (slow) and 1 (immediate). Must
 * be larger than 0.
 *
 * @type {number}
 * @const
 */
camera.util.SmoothScroller.SPEED = 0.1;

camera.util.SmoothScroller.prototype = {
  get element() {
    return this.element_;
  },
  get animating() {
    return !!this.targetPosition_;
  }
};

/**
 * Animates one frame.
 * @private
 */
camera.util.SmoothScroller.prototype.animate_ = function() {
  var dx = (this.targetPosition_[0] - this.currentPosition_[0]);
  var dy = (this.targetPosition_[1] - this.currentPosition_[1]);

  // If the remaining distance is very small, then finish.
  if (Math.abs(dx) < 0.5 && Math.abs(dy) < 0.5) {
    this.element_.scrollLeft = Math.round(this.targetPosition_[0]);
    this.element_.scrollTop = Math.round(this.targetPosition_[1]);
    this.targetPosition_ = null;
    return;
  }

  var previousPosition = this.currentPosition_.slice(0);
  this.currentPosition_[0] += dx * camera.util.SmoothScroller.SPEED;
  this.currentPosition_[1] += dy * camera.util.SmoothScroller.SPEED;

  // Terminate the animation if interrupted by a manual scroll change.
  if (this.element_.scrollLeft != this.lastScrollPosition_[0] ||
      this.element_.scrollTop != this.lastScrollPosition_[1]) {
    this.targetPosition_ = null;
    return;
  }

  // Calculate the new values to be set.
  var setX = Math.round(this.currentPosition_[0]);
  var setY = Math.round(this.currentPosition_[1]);

  // Apply the new values only if they are different than the previous ones.
  this.element_.scrollLeft = setX;
  this.element_.scrollTop = setY;

  // Finish if the update didn't change anything, but should.
  if (this.element_.scrollLeft != setX &&
      setX != Math.round(previousPosition[0]) &&
      this.element_.scrollTop != setY &&
      setY != Math.round(previousPosition[1])) {
    this.targetPosition_ = null;
    return;
  }

  // Remember the last set position and continue the animation.
  this.lastScrollPosition_[0] = this.element_.scrollLeft;
  this.lastScrollPosition_[1] = this.element_.scrollTop;
  webkitRequestAnimationFrame(this.animate_.bind(this));
};

/**
 * Scrolls smoothly to specified position.
 *
 * @param {number} x X Target scrollLeft value.
 * @param {number} y Y Target scrollTop value.
 */
camera.util.SmoothScroller.prototype.scrollTo = function(x, y) {
  var isAnimating = this.animating;

  this.targetPosition_ = [x, y];
  this.lastScrollPosition_ =
      [this.element_.scrollLeft, this.element_.scrollTop];
  this.currentPosition_ = this.lastScrollPosition_.slice(0);

  // It is not animating, then fire an animation frame.
  if (!isAnimating)
    this.animate_();
};

