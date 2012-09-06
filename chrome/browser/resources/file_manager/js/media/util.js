// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A controller class detects mouse inactivity and hides "tool" elements.
 *
 * @param {Element} container The main DOM container.
 * @param {number} opt_timeout Hide timeout in ms.
 * @param {function():boolean=} opt_toolsActive Function that returns |true|
 *     if the tools are active and should not be hidden.
 * @constructor
 */
function MouseInactivityWatcher(container, opt_timeout, opt_toolsActive) {
  this.container_ = container;
  this.timeout_ = opt_timeout || MouseInactivityWatcher.DEFAULT_TIMEOUT;
  this.toolsActive_ = opt_toolsActive || function() { return false };

  this.onTimeoutBound_ = this.onTimeout_.bind(this);
  this.timeoutID_ = null;
  this.mouseOverTool_ = false;

  this.clientX_ = 0;
  this.clientY_ = 0;
  this.container_.addEventListener('mousemove', this.onMouseMove_.bind(this));
}

/**
 * Default inactivity timeout.
 */
MouseInactivityWatcher.DEFAULT_TIMEOUT = 3000;

/**
 * @param {boolean} on True if show, false if hide.
 */
MouseInactivityWatcher.prototype.showTools = function(on) {
  if (on)
    this.container_.setAttribute('tools', 'true');
  else
    this.container_.removeAttribute('tools');
};

/**
 * @param {Element} element DOM element
 * @return {boolean} True if the element is a tool. Tools should never be hidden
 *   while the mouse is over one of them.
 */
MouseInactivityWatcher.prototype.isToolElement = function(element) {
  return element.classList.contains('tool');
};

/**
 * To be called when the user started activity. Shows the tools
 * and cancels the countdown.
 */
MouseInactivityWatcher.prototype.startActivity = function() {
  this.showTools(true);

  if (this.timeoutID_) {
    clearTimeout(this.timeoutID_);
    this.timeoutID_ = null;
  }
};

/**
 * Called when user activity has stopped. Re-starts the countdown.
 * @param {number} opt_timeout Timeout.
 */
MouseInactivityWatcher.prototype.stopActivity = function(opt_timeout) {
  if (this.mouseOverTool_ || this.toolsActive_())
    return;

  if (this.timeoutID_)
    clearTimeout(this.timeoutID_);

  this.timeoutID_ = setTimeout(
      this.onTimeoutBound_, opt_timeout || this.timeout_);
};

/**
 * Mouse move handler.
 *
 * @param {Event} e Event.
 * @private
 */
MouseInactivityWatcher.prototype.onMouseMove_ = function(e) {
  if (this.clientX_ == e.clientX && this.clientY_ == e.clientY) {
    // The mouse has not moved, must be the cursor change triggered by
    // some of the attributes on the root container. Ignore the event.
    return;
  }
  this.clientX_ = e.clientX;
  this.clientY_ = e.clientY;

  this.startActivity();

  this.mouseOverTool_ = false;
  for (var elem = e.target; elem != this.container_; elem = elem.parentNode) {
    if (this.isToolElement(elem)) {
      this.mouseOverTool_ = true;
      break;
    }
  }

  this.stopActivity();
};

/**
 * Timeout handler.
 * @private
 */
MouseInactivityWatcher.prototype.onTimeout_ = function() {
  this.timeoutID_ = null;
  if (!this.toolsActive_())
    this.showTools(false);
};
