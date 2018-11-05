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
 * Creates the Dialog view controller.
 * @param {cca.Router} router View router to switch views.
 * @extends {cca.View}
 * @constructor
 */
cca.views.Dialog = function(router) {
  cca.View.call(
      this, router, document.querySelector('#dialog'), 'dialog');

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.positiveButton_ = document.querySelector('#dialog-positive-button');

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.negativeButton_ = document.querySelector('#dialog-negative-button');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.messageElement_ = document.querySelector('#dialog-msg');

  // End of properties, seal the object.
  Object.seal(this);

  // Register handler on the buttons.
  this.positiveButton_.addEventListener(
      'click', this.onPositiveButtonClicked_.bind(this));
  this.negativeButton_.addEventListener(
      'click', this.onNegativeButtonClicked_.bind(this));
};

/**
 * Type of the dialog.
 * @enum {number}
 */
cca.views.Dialog.Type = Object.freeze({
  CONFIRMATION: 0,
  ALERT: 1,
});

cca.views.Dialog.prototype = {
  __proto__: cca.View.prototype,
};

/**
 * Enters the view. Assumes, that the corresponding arguments are provided
 * according to each type:
 * CONFIRMATION type with 'message' argument,
 * ALERT type with 'message' argument,
 * @param {Object=} opt_arguments Arguments for the dialog.
 * @override
 */
cca.views.Dialog.prototype.onEnter = function(opt_arguments) {
  // Update the element's text content and hide it if there is no text content.
  var updateElement = function(element, text) {
    if (text) {
      element.textContent = text.content ? text.content :
          chrome.i18n.getMessage(text.name);
      element.hidden = false;
    } else {
      element.hidden = true;
    }
  };

  var positiveText = null;
  var negativeText = null;
  var messageText = null;
  switch (opt_arguments.type) {
    case cca.views.Dialog.Type.CONFIRMATION:
      positiveText = { name: 'dialogOKButton' };
      negativeText = { name: 'dialogCancelButton' };
      messageText = { content: opt_arguments.message };
      break;
    case cca.views.Dialog.Type.ALERT:
      positiveText = { name: 'dialogOKButton' };
      messageText = { content: opt_arguments.message };
      break;
  }
  updateElement(this.positiveButton_, positiveText);
  updateElement(this.negativeButton_, negativeText);
  updateElement(this.messageElement_, messageText);
};

/**
 * @override
 */
cca.views.Dialog.prototype.onActivate = function() {
  if (!this.positiveButton_.hidden)
    this.positiveButton_.focus();
};

/**
 * Handles clicking on the positive button.
 * @param {Event} event Click event.
 * @private
 */
cca.views.Dialog.prototype.onPositiveButtonClicked_ = function(event) {
  this.router.back({isPositive: true});
};

/**
 * Handles clicking on the negative button.
 * @param {Event} event Click event.
 * @private
 */
cca.views.Dialog.prototype.onNegativeButtonClicked_ = function(event) {
  this.closeDialog_();
};

/**
 * Dismisses the dialog without returning a positive result.
 * @private
 */
cca.views.Dialog.prototype.closeDialog_ = function() {
  this.router.back({isPositive: false});
};

/**
 * @override
 */
cca.views.Dialog.prototype.onKeyPressed = function(event) {
  if (cca.util.getShortcutIdentifier(event) == 'Escape') {
    this.closeDialog_();
    event.preventDefault();
  }
};
