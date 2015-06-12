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
 * Creates a UI Manager to handle transitioning of toolbars.
 * @constructor
 * @param {Object} window The window containing the UI.
 * @param {Object} toolbar The top toolbar element.
 * @param {Object} zoomToolbar The zoom toolbar element.
 */
function UiManager(window, toolbar, zoomToolbar) {
  this.window_ = window;
  this.toolbar_ = toolbar;
  this.zoomToolbar_ = zoomToolbar;

  this.uiTimeout_ = null;
  this.keepOpen_ = false;

  var userInputs = ['keydown', 'mousemove'];
  for (var i = 0; i < userInputs.length; i++)
    this.window_.addEventListener(userInputs[i], this.handleEvent.bind(this));
}

UiManager.prototype = {
  handleEvent: function(e) {
    this.keepOpen_ = shouldKeepUiOpen(e);
    if (e.type != 'mousemove' || this.keepOpen_ || isHighVelocityMouseMove(e))
      this.showUi_();
  },

  /**
   * @private
   * Display the toolbar and any pane that was previously opened.
   */
  showUi_: function() {
    this.toolbar_.show();
    this.zoomToolbar_.show();
    this.hideUiAfterTimeout();
  },

  /**
   * @private
   * Hide the toolbar and any pane that was previously opened.
   */
  hideUi_: function() {
    if (!this.keepOpen_) {
      this.toolbar_.hide();
      this.zoomToolbar_.hide();
    }
  },

  /**
   * Hide the toolbar after the HIDE_TIMEOUT has elapsed.
   */
  hideUiAfterTimeout: function() {
    if (this.uiTimeout_)
      clearTimeout(this.uiTimeout_);
    this.uiTimeout_ = setTimeout(this.hideUi_.bind(this), HIDE_TIMEOUT);
  }
};
