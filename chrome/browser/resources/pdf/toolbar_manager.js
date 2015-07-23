// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** Idle time in ms before the UI is hidden. */
var HIDE_TIMEOUT = 2000;
/** Velocity required in a mousemove to reveal the UI (pixels/sample). */
var SHOW_VELOCITY = 20;
/** Distance from the top or right of the screen required to reveal the UI. */
var EDGE_REVEAL = 100;

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
 * Whether the mouse is close enough to the edge of the screen to keep the
 * toolbars open.
 * @param {Event} e Event to test.
 * @return {boolean} true if the mouse is close to the top or right of the
 * screen.
 */
function shouldKeepUiOpen(e) {
  return (e.y < EDGE_REVEAL || e.x > window.innerWidth - EDGE_REVEAL);
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
  this.keepOpen_ = false;

  this.window_.addEventListener('resize', this.resizeDropdowns_.bind(this));
  this.resizeDropdowns_();
}

ToolbarManager.prototype = {

  showToolbarsForMouseMove: function(e) {
    this.keepOpen_ = shouldKeepUiOpen(e);
    if (this.keepOpen_ || isHighVelocityMouseMove(e))
      this.showToolbars();
    this.hideToolbarsAfterTimeout();
  },

  /**
   * Display both UI toolbars.
   */
  showToolbars: function() {
    this.toolbar_.show();
    this.zoomToolbar_.show();
  },

  /**
   * Check if the toolbars are able to be closed, and close them if they are.
   * Toolbars may be kept open based on mouse/keyboard activity and active
   * elements.
   */
  hideToolbarsIfAllowed: function() {
    if (!(this.keepOpen_ || this.toolbar_.shouldKeepOpen())) {
      this.toolbar_.hide();
      this.zoomToolbar_.hide();
    }
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
    if (!this.toolbar_.hideDropdowns())
      this.hideToolbarsIfAllowed();
  },

  /**
   * Updates the size of toolbar dropdowns based on the positions of the rest of
   * the UI.
   * @private
   */
  resizeDropdowns_: function() {
    var lowerBound = this.window_.innerHeight - this.zoomToolbar_.clientHeight;
    this.toolbar_.setDropdownLowerBound(lowerBound);
  }
};
