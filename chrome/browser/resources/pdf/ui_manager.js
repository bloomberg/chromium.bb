// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var HIDE_TIMEOUT = 2000;

/**
 * Creates a UI Manager to handle transitioning of toolbars and panes.
 * @constructor
 * @param {Object} window The window containing the UI.
 * @param {Object} toolbar The toolbar element.
 * @param {Array} panes The panes that may be pulled in.
 */
function UiManager(window, toolbar, panes) {
  this.window_ = window;
  this.toolbar_ = toolbar;
  this.panes_ = panes;

  this.uiTimeout_ = null;

  var userInputs = ['click', 'keydown', 'mousemove', 'scroll'];
  for (var i = 0; i < userInputs.length; i++)
    this.window_.addEventListener(userInputs[i], this.showUi_.bind(this));
}

UiManager.prototype = {
  /**
   * @private
   * Display the toolbar and any pane that was previously opened.
   */
  showUi_: function() {
    this.toolbar_.show();
    for (var i = 0; i < this.panes_.length; i++)
      this.panes_[i].showIfOpenedByUser();

    this.hideUiAfterTimeout();
  },

  /**
   * @private
   * Hide the toolbar and any pane that was previously opened.
   */
  hideUi_: function() {
    this.toolbar_.hide();
    for (var i = 0; i < this.panes_.length; i++)
      this.panes_[i].hideIfOpenedByUser();
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
