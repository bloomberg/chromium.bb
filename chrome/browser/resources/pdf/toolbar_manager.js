// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** Idle time in ms before the UI is hidden. */
var HIDE_TIMEOUT = 2000;
/** Time in ms after force hide before toolbar is shown again. */
var FORCE_HIDE_TIMEOUT = 1000;
/** Velocity required in a mousemove to reveal the UI (pixels/sample). */
var SHOW_VELOCITY = 25;
/** Distance from the top of the screen required to reveal the toolbars. */
var TOP_TOOLBAR_REVEAL_DISTANCE = 100;
/** Distance from the bottom-right of the screen required to reveal toolbars. */
var SIDE_TOOLBAR_REVEAL_DISTANCE_RIGHT = 150;
var SIDE_TOOLBAR_REVEAL_DISTANCE_BOTTOM = 250;

/**
 * Whether a mousemove event is high enough velocity to reveal the toolbars.
 * @param {MouseEvent} e Event to test.
 * @return {boolean} true if the event is a high velocity mousemove, false
 * otherwise.
 */
function isHighVelocityMouseMove(e) {
  return e.type == 'mousemove' &&
         e.movementX * e.movementX + e.movementY * e.movementY >
             SHOW_VELOCITY * SHOW_VELOCITY;
}

/**
 * @param {MouseEvent} e Event to test.
 * @return {boolean} True if the mouse is close to the top of the screen.
 */
function isMouseNearTopToolbar(e) {
  return e.y < TOP_TOOLBAR_REVEAL_DISTANCE;
}

/**
 * @param {MouseEvent} e Event to test.
 * @return {boolean} True if the mouse is close to the bottom-right of the
 * screen.
 */
function isMouseNearSideToolbar(e) {
  return e.x > window.innerWidth - SIDE_TOOLBAR_REVEAL_DISTANCE_RIGHT &&
         e.y > window.innerHeight - SIDE_TOOLBAR_REVEAL_DISTANCE_BOTTOM;
}

/**
 * Constructs a Toolbar Manager, responsible for co-ordinating between multiple
 * toolbar elements.
 * @constructor
 * @param {Object} window The window containing the UI.
 * @param {Object} toolbar The top toolbar element.
 * @param {Object} zoomToolbar The zoom toolbar element.
 */
function ToolbarManager(window, toolbar, zoomToolbar) {
  this.window_ = window;
  this.toolbar_ = toolbar;
  this.zoomToolbar_ = zoomToolbar;

  this.toolbarTimeout_ = null;
  this.isMouseNearTopToolbar_ = false;
  this.isMouseNearSideToolbar_ = false;

  this.sideToolbarAllowedOnly_ = false;
  this.sideToolbarAllowedOnlyTimer_ = null;

  this.window_.addEventListener('resize', this.resizeDropdowns_.bind(this));
  this.resizeDropdowns_();
}

ToolbarManager.prototype = {

  showToolbarsForMouseMove: function(e) {
    this.isMouseNearTopToolbar_ = this.toolbar_ && isMouseNearTopToolbar(e);
    this.isMouseNearSideToolbar_ = isMouseNearSideToolbar(e);

    // Allow the top toolbar to be shown if the mouse moves away from the side
    // toolbar (as long as the timeout has elapsed).
    if (!this.isMouseNearSideToolbar_ && !this.sideToolbarAllowedOnlyTimer_)
      this.sideToolbarAllowedOnly_ = false;

    // Allow the top toolbar to be shown if the mouse moves to the top edge.
    if (this.isMouseNearTopToolbar_)
      this.sideToolbarAllowedOnly_ = false;

    // Show the toolbars if the mouse is near the top or right of the screen or
    // if the mouse moved fast.
    if (this.isMouseNearTopToolbar_ || this.isMouseNearSideToolbar_ ||
        isHighVelocityMouseMove(e)) {
      if (this.sideToolbarAllowedOnly_)
        this.zoomToolbar_.show();
      else
        this.showToolbars();
    }
    this.hideToolbarsAfterTimeout();
  },

  /**
   * Display both UI toolbars.
   */
  showToolbars: function() {
    if (this.toolbar_)
      this.toolbar_.show();
    this.zoomToolbar_.show();
  },

  /**
   * Hide toolbars after a delay, regardless of the position of the mouse.
   * Intended to be called when the mouse has moved out of the parent window.
   */
  hideToolbarsForMouseOut: function() {
    this.isMouseNearTopToolbar_ = false;
    this.isMouseNearSideToolbar_ = false;
    this.hideToolbarsAfterTimeout();
  },

  /**
   * Check if the toolbars are able to be closed, and close them if they are.
   * Toolbars may be kept open based on mouse/keyboard activity and active
   * elements.
   */
  hideToolbarsIfAllowed: function() {
    if (this.isMouseNearSideToolbar_ || this.isMouseNearTopToolbar_)
      return;

    if (this.toolbar_ && this.toolbar_.shouldKeepOpen())
      return;

    if (this.toolbar_)
      this.toolbar_.hide();
    this.zoomToolbar_.hide();
  },

  /**
   * Hide the toolbar after the HIDE_TIMEOUT has elapsed.
   */
  hideToolbarsAfterTimeout: function() {
    if (this.toolbarTimeout_)
      clearTimeout(this.toolbarTimeout_);
    this.toolbarTimeout_ =
        setTimeout(this.hideToolbarsIfAllowed.bind(this), HIDE_TIMEOUT);
  },

  /**
   * Hide the 'topmost' layer of toolbars. Hides any dropdowns that are open, or
   * hides the basic toolbars otherwise.
   */
  hideSingleToolbarLayer: function() {
    if (!this.toolbar_ || !this.toolbar_.hideDropdowns())
      this.hideToolbarsIfAllowed();
  },

  /**
   * Hide the top toolbar and keep it hidden until both:
   * - The mouse is moved away from the right side of the screen
   * - 1 second has passed.
   *
   * The top toolbar can be immediately re-opened by moving the mouse to the top
   * of the screen.
   */
  forceHideTopToolbar: function() {
    if (!this.toolbar_)
      return;
    this.toolbar_.hide();
    this.sideToolbarAllowedOnly_ = true;
    this.sideToolbarAllowedOnlyTimer_ = this.window_.setTimeout(function() {
      this.sideToolbarAllowedOnlyTimer_ = null;
    }.bind(this), FORCE_HIDE_TIMEOUT);
  },

  /**
   * Updates the size of toolbar dropdowns based on the positions of the rest of
   * the UI.
   * @private
   */
  resizeDropdowns_: function() {
    if (!this.toolbar_)
      return;
    var lowerBound = this.window_.innerHeight - this.zoomToolbar_.clientHeight;
    this.toolbar_.setDropdownLowerBound(lowerBound);
  }
};
