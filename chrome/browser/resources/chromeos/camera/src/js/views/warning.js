// Copyright 2018 The Chromium Authors. All rights reserved.
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
 * Creates the warning-view controller.
 * @extends {cca.views.View}
 * @constructor
 */
cca.views.Warning = function() {
  cca.views.View.call(this, '#warning');

  /**
   * @type {Array<string>}
   * @private
   */
  this.errorNames_ = [];

  // End of properties, seal the object.
  Object.seal(this);
};

cca.views.Warning.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * Updates the error message for the latest error-name in the stack.
 * @private
 */
cca.views.Warning.prototype.updateMessage_ = function() {
  var message = '';
  switch (this.errorNames_[this.errorNames_.length - 1]) {
    case 'no-camera':
      message = 'error_msg_no_camera';
      break;
    case 'filesystem-failure':
      message = 'error_msg_file_system_failed';
      break;
  }
  document.querySelector('#error-msg').textContent =
      chrome.i18n.getMessage(message);
};

/**
 * @param {string} name Name of the error.
 * @override
 */
cca.views.Warning.prototype.entering = function(name) {
  // Remove the error-name from the stack to avoid duplication. Then make the
  // error-name the latest one to show its message.
  var index = this.errorNames_.indexOf(name);
  if (index != -1) {
    this.errorNames_.splice(index, 1);
  }
  this.errorNames_.push(name);
  this.updateMessage_();
};

/**
 * @param {string} name Recovered error-name for leaving the view.
 * @override
 */
cca.views.Warning.prototype.leaving = function(name) {
  // Remove the recovered error from the stack but don't leave the view until
  // there is no error left in the stack.
  var index = this.errorNames_.indexOf(name);
  if (index != -1) {
    this.errorNames_.splice(index, 1);
  }
  if (this.errorNames_.length) {
    this.updateMessage_();
    return false;
  }
  document.querySelector('#error-msg').textContent = '';
  return true;
};
