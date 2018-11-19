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
 * @extends {cca.views.View}
 * @constructor
 */
cca.views.Dialog = function() {
  cca.views.View.call(this, '#dialog');

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

cca.views.Dialog.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * @param {string} message Message of the dialog.
 * @param {boolean} cancellable Whether the dialog is cancellable.
 * @override
 */
cca.views.Dialog.prototype.entering = function(message, cancellable) {
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
  var messageText = { content: message };
  var positiveText = { name: 'dialogOKButton' };
  var negativeText = cancellable ? { name: 'dialogCancelButton' } : null;
  updateElement(this.messageElement_, messageText);
  updateElement(this.positiveButton_, positiveText);
  updateElement(this.negativeButton_, negativeText);
};

/**
 * @override
 */
cca.views.Dialog.prototype.focus = function() {
  if (!this.positiveButton_.hidden) {
    this.positiveButton_.focus();
  }
};

/**
 * Handles clicking on the positive button.
 * @param {Event} event Click event.
 * @private
 */
cca.views.Dialog.prototype.onPositiveButtonClicked_ = function(event) {
  this.leave(true);
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
  this.leave(false);
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
