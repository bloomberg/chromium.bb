// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Base controller of a view for views' navigation sessions (cca.nav).
 * @param {string} selector Selector text of the view's root element.
 * @constructor
 */
cca.views.View = function(selector) {
  /**
   * @type {HTMLElement}
   * @private
   */
  this.rootElement_ = document.querySelector(selector);

  /**
   * @type {Promise<*>}
   * @private
   */
  this.session_ = null;
};

cca.views.View.prototype = {
  get root() {
    return this.rootElement_;
  },
};

/**
 * Handles key pressed events.
 * @param {Event} event Key event.
 */
cca.views.View.prototype.onKeyPressed = function(event) {
};

/**
 * Focuses the default element on the view if applicable.
 */
cca.views.View.prototype.focus = function() {
};

/**
 * Layouts the view.
 */
cca.views.View.prototype.layout = function() {
};

/**
 * Hook called when entering the view.
 * @param {...*} args Optional rest parameters for entering the view.
 */
cca.views.View.prototype.entering = function(...args) {
};

/**
 * Enters the view.
 * @param {...*} args Optional rest parameters for entering the view.
 * @return {!Promise<*>} Promise for the navigation session.
 */
cca.views.View.prototype.enter = function(...args) {
  // The session is started by entering the view and ended by leaving the view.
  if (!this.session_) {
    var end;
    this.session_ = new Promise((resolve) => {
      end = resolve;
    });
    this.session_.end = (result) => {
      end(result);
    };
  }
  this.entering(...args);
  return this.session_;
};

/**
 * Hook called when leaving the view.
 */
cca.views.View.prototype.leaving = function() {
};

/**
 * Leaves the view.
 * @param {*=} result Optional result for the ended navigation session.
 */
cca.views.View.prototype.leave = function(result) {
  if (this.session_) {
    this.leaving();
    this.session_.end(result);
    this.session_ = null;
  }
};
