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
 * @param {=boolean} opt_immediate True for immediate scroll.
 */
camera.util.ensureVisible = function(element, scroller, opt_immediate) {
  var parent = scroller.element;
  var scrollLeft = parent.scrollLeft;
  var scrollTop = parent.scrollTop;

  if (element.offsetTop < parent.scrollTop)
    scrollTop = Math.round(element.offsetTop - element.offsetHeight * 0.5);
  if (element.offsetTop + element.offsetHeight >
      scrollTop + parent.offsetHeight) {
    scrollTop = Math.round(element.offsetTop + element.offsetHeight * 1.5 -
        parent.offsetHeight);
  }
  if (element.offsetLeft < parent.scrollLeft)
    scrollLeft = Math.round(element.offsetLeft - element.offsetWidth * 0.5);
  if (element.offsetLeft + element.offsetWidth >
      scrollLeft + parent.offsetWidth) {
    scrollLeft = Math.round(element.offsetLeft + element.offsetWidth * 1.5 -
        parent.offsetWidth);
  }
  if (scrollTop != parent.scrollTop ||
      scrollLeft != parent.scrollLeft) {
    if (opt_immediate) {
      parent.scrollLeft = scrollLeft;
      parent.scrollTop = scrollTop;
    } else {
      scroller.scrollTo(scrollLeft, scrollTop);
    }
  }
};

/**
 * Scrolls the parent of the element so the element is centered.
 *
 * @param {HTMLElement} element Element to be visible.
 * @param {camera.util.SmoothScroller} scroller Scroller to be used.
 * @param {=boolean} opt_immediate True for immediate scroll.
 */
camera.util.scrollToCenter = function(element, scroller, opt_immediate) {
  var parent = scroller.element;
  var scrollLeft = Math.round(element.offsetLeft + element.offsetWidth / 2 -
    parent.offsetWidth / 2);
  var scrollTop = Math.round(element.offsetTop + element.offsetHeight / 2 -
    parent.offsetHeight / 2);

  if (scrollTop != parent.scrollTop ||
      scrollLeft != parent.scrollLeft) {
    scroller.scrollTo(scrollLeft, scrollTop);
  }
  if (scrollTop != parent.scrollTop ||
      scrollLeft != parent.scrollLeft) {
    if (opt_immediate) {
      parent.scrollLeft = scrollLeft;
      parent.scrollTop = scrollTop;
    } else {
      scroller.scrollTo(scrollLeft, scrollTop);
    }
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

/**
 * Runs asynchronous closures in a queue.
 * @constructor
 */
camera.util.Queue = function() {
  /**
   * @type {Array.<function(function())>}
   * @private
   */
  this.closures_ = [];

  /**
   * @type {boolean}
   * @private
   */
  this.running_ = false;

  // End of properties. Seal the object.
  Object.seal(this);
};

/**
 * Runs a task within the queue.
 * @param {function(function())} closure Closure to be run with a completion
 *     callback.
 */
camera.util.Queue.prototype.run = function(closure) {
  this.closures_.push(closure);
  if (!this.running_)
    this.continue_();
};

/**
 * Continues executing further enqueued closures, or stops the queue if nothing
 * pending.
 * @private
 */
camera.util.Queue.prototype.continue_ = function() {
  if (!this.closures_.length) {
    this.running_ = false;
    return;
  }

  this.running_ = true;
  var closure = this.closures_.shift();
  closure(this.continue_.bind(this));
};

/**
 * Tracks the mouse for click and move, and the touch screen for touches. If
 * any of these are detected, then the callback is called.
 *
 * @type {HTMLElement} element Element to be monitored.
 * @type {function()} callback Callback triggered on events detected.
 * @constructor
 */
camera.util.PointerTracker = function(element, callback) {
  /**
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * @type {function()}
   * @private
   */
  this.callback_ = callback;

  /**
   * @type {Array.<number>}
   * @private
   */
  this.lastMousePosition_ = null;

  // End of properties. Seal the object.
  Object.seal(this);

  // Add the event listeners.
  this.element_.addEventListener('mousedown', this.onMouseDown_.bind(this));
  this.element_.addEventListener('mousemove', this.onMouseMove_.bind(this));
  this.element_.addEventListener('touchstart', this.onTouchStart_.bind(this));
  this.element_.addEventListener('touchmove', this.onTouchMove_.bind(this));
};

/**
 * Handles the mouse down event.
 *
 * @type {Event} event Mouse down event.
 * @private
 */
camera.util.PointerTracker.prototype.onMouseDown_ = function(event) {
  this.callback_();
  this.lastMousePosition_ = [event.screenX, event.screenY];
};

/**
 * Handles the mouse move event.
 *
 * @type {Event} event Mouse move event.
 * @private
 */
camera.util.PointerTracker.prototype.onMouseMove_ = function(event) {
  // Ignore mouse events, which are invoked on the same position, but with
  // changed client coordinates. This will happen eg. when scrolling. We should
  // ignore them, since they are not invoked by an actual mouse move.
  if (this.lastMousePosition_ && this.lastMousePosition_[0] == event.screenX &&
      this.lastMousePosition_[1] == event.screenY) {
    return;
  }

  this.callback_();
  this.lastMousePosition_ = [event.screenX, event.screenY];
};

/**
 * Handles the touch start event.
 *
 * @type {Event} event Touch start event.
 * @private
 */
camera.util.PointerTracker.prototype.onTouchStart_ = function(event) {
  this.callback_();
};

/**
 * Handles the touch move event.
 *
 * @type {Event} event Touch move event.
 * @private
 */
camera.util.PointerTracker.prototype.onTouchMove_ = function(event) {
  this.callback_();
};

/**
 * Tracks scrolling and calls a callback, when scrolling is started and ended.
 *
 * @param {HTMLElement} element Element to be tracked.
 * @param {function()} onScrollStarted Callback called when scrolling is
 *     started.
 * @param {function()} onScrollEnded Callback called when scrolling is ended.
 * @constructor
 */
camera.util.ScrollTracker = function(element, onScrollStarted, onScrollEnded) {
  /**
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * @type {function()}
   * @private
   */
  this.onScrollStarted_ = onScrollStarted;

  /**
   * @type {function()}
   * @private
   */
  this.onScrollEnded_ = onScrollEnded;

  /**
   * Timer to probe for scroll changes, every 100 ms.
   * @type {?number}
   * @private
   */
  this.timer_ = null;

  /**
   * @type {boolean}
   * @private
   */
  this.scrolling_ = false;

  /**
   * @type {Array.<number>}
   * @private
   */
  this.lastScrollPosition_ = [0, 0];

  /**
   * Whether the touch screen is currently touched.
   * @type {boolean}
   * @private
   */
  this.touchPressed_ = false;

  /**
   * Whether the touch screen is currently touched.
   * @type {boolean}
   * @private
   */
  this.mousePressed_ = false;

  // End of properties. Seal the object.
  Object.seal(this);

  // Register event handlers.
  this.element_.addEventListener('mousedown', this.onMouseDown_.bind(this));
  this.element_.addEventListener('touchstart', this.onTouchStart_.bind(this));
  window.addEventListener('mouseup', this.onMouseUp_.bind(this));
  window.addEventListener('touchend', this.onTouchEnd_.bind(this));
};

/**
 * Handles pressing the mouse button.
 * @param {Event} event Mouse down event.
 * @private
 */
camera.util.ScrollTracker.prototype.onMouseDown_ = function(event) {
  this.mousePressed_ = true;
};

/**
 * Handles releasing the mouse button.
 * @param {Event} event Mouse up event.
 * @private
 */
camera.util.ScrollTracker.prototype.onMouseUp_ = function(event) {
  this.mousePressed_ = false;
};

/**
 * Handles touching the screen.
 * @param {Event} event Mouse down event.
 * @private
 */
camera.util.ScrollTracker.prototype.onTouchStart_ = function(event) {
  this.touchPressed_ = true;
};

/**
 * Handles releasing touching of the screen.
 * @param {Event} event Mouse up event.
 * @private
 */
camera.util.ScrollTracker.prototype.onTouchEnd_ = function(event) {
  this.touchPressed_ = false;
};

/**
 * Starts monitoring.
 */
camera.util.ScrollTracker.prototype.start = function() {
  if (this.timer_ !== null)
    return;
  this.timer_ = setInterval(this.probe_.bind(this), 100);
};

/**
 * Stops monitoring.
 */
camera.util.ScrollTracker.prototype.stop = function() {
  if (this.timer_ === null)
    return;
  clearTimeout(this.timer_);
  this.timer_ = null;
};

/**
 * Probes for scrolling changes.
 * @private
 */
camera.util.ScrollTracker.prototype.probe_ = function() {
  var scrollLeft = this.element_.scrollLeft;
  var scrollTop = this.element_.scrollTop;

  if (scrollLeft != this.lastScrollPosition_[0] ||
      scrollTop != this.lastScrollPosition_[1]) {
    if (!this.scrolling_)
      this.onScrollStarted_();
    this.scrolling_ = true;
  } else {
    if (!this.mousePressed_ && !this.touchPressed_ && this.scrolling_) {
      this.onScrollEnded_();
      this.scrolling_ = false;
    }
  }

  this.lastScrollPosition_ = [scrollLeft, scrollTop];
};
