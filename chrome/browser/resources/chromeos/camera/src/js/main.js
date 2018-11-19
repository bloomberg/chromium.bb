// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Creates the Camera App main object.
 * @param {number} aspectRatio Aspect ratio of app window when launched.
 * @constructor
 */
cca.App = function(aspectRatio) {
  /**
   * @type {cca.models.Gallery}
   * @private
   */
  this.model_ = new cca.models.Gallery();

  /**
   * @type {cca.views.Camera}
   * @private
   */
  this.cameraView_ = new cca.views.Camera(
      this.model_, this.onWindowResize_.bind(this));

  /**
   * @type {cca.views.Browser}
   * @private
   */
  this.browserView_ = new cca.views.Browser(this.model_);

  /**
   * @type {cca.views.Dialog}
   * @private
   */
  this.dialogView_ = new cca.views.Dialog();

  /**
   * @type {?number}
   * @private
   */
  this.resizeWindowTimeout_ = null;

  /**
   * @type {number}
   * @private
   */
  this.aspectRatio_ = aspectRatio;

  // End of properties. Seal the object.
  Object.seal(this);

  document.body.addEventListener('keydown', this.onKeyPressed_.bind(this));
  window.addEventListener('resize', this.onWindowResize_.bind(this, null));

  document.title = chrome.i18n.getMessage('name');
};

/**
 * Starts the app by preparing views/model and opening the camera-view.
 */
cca.App.prototype.start = function() {
  cca.util.setupElementsAria();
  cca.tooltip.setup();
  cca.nav.setup([this.cameraView_, this.browserView_, this.dialogView_]);
  cca.models.FileSystem.initialize(() => {
    // Prompt to migrate pictures if needed.
    var message = chrome.i18n.getMessage('migratePicturesMsg');
    return cca.nav.dialog(message, false).then((acked) => {
      if (!acked) {
        throw 'no-migrate';
      }
    });
  }).then(() => {
    // Prepare the views/model and open camera-view.
    this.cameraView_.prepare();
    this.browserView_.prepare();
    this.model_.load([this.cameraView_.galleryButton, this.browserView_]);
    cca.nav.camera();
  }).catch((error) => {
    console.error(error);
    if (error == 'no-migrate') {
      chrome.app.window.current().close();
      return;
    }
    cca.App.onError('filesystem-failure', 'errorMsgFileSystemFailed');
  });
};

/**
 * Resizes the window to match the last known aspect ratio if applicable.
 * @private
 */
cca.App.prototype.resizeByAspectRatio_ = function() {
  // Don't update window size if it's maximized or fullscreen.
  if (cca.util.isWindowFullSize()) {
    return;
  }

  // Keep the width fixed and calculate the height by the aspect ratio.
  // TODO(yuli): Update min-width for resizing at portrait orientation.
  var appWindow = chrome.app.window.current();
  var inner = appWindow.innerBounds;
  var innerW = inner.minWidth;
  var innerH = Math.round(innerW / this.aspectRatio_);

  // Limit window resizing capability by setting min-height. Don't limit
  // max-height here as it may disable maximize/fullscreen capabilities.
  // TODO(yuli): Revise if there is an alternative fix.
  inner.minHeight = innerH;

  inner.width = innerW;
  inner.height = innerH;
};

/**
 * Handles resizing window/views for size or aspect ratio changes.
 * @param {number=} aspectRatio Aspect ratio changed.
 * @private
 */
cca.App.prototype.onWindowResize_ = function(aspectRatio) {
  if (this.resizeWindowTimeout_) {
    clearTimeout(this.resizeWindowTimeout_);
    this.resizeWindowTimeout_ = null;
  }
  if (aspectRatio) {
    // Update window size immediately for changed aspect ratio.
    this.aspectRatio_ = aspectRatio;
    this.resizeByAspectRatio_();
  } else {
    // Don't further resize window during resizing for smooth UX.
    this.resizeWindowTimeout_ = setTimeout(() => {
      this.resizeWindowTimeout_ = null;
      this.resizeByAspectRatio_();
    }, 500);
  }
  cca.nav.onWindowResized();
};

/**
 * Handles pressed keys.
 * @param {Event} event Key press event.
 * @private
 */
cca.App.prototype.onKeyPressed_ = function(event) {
  cca.tooltip.hide(); // Hide shown tooltip on any keypress.
  if (cca.util.getShortcutIdentifier(event) == 'BrowserBack') {
    chrome.app.window.current().minimize();
    return;
  }
  cca.nav.onKeyPressed(event);
};

/**
 * Shows an error message.
 * @param {string} identifier Identifier of the error.
 * @param {string} message Message for the error.
 */
cca.App.onError = function(identifier, message) {
  // TODO(yuli): Implement error-identifier to look up messages/hints and handle
  // multiple errors. Make 'error' a view to block buttons on other views.
  document.body.classList.add('has-error');
  // Use setTimeout to wait for error-view to be visible by screen reader.
  setTimeout(() => {
    document.querySelector('#error-msg').textContent =
        chrome.i18n.getMessage(message) || message;
  }, 0);
};

/**
 * Removes the error message when an error goes away.
 * @param {string} identifier Identifier of the error.
 */
cca.App.onErrorRecovered = function(identifier) {
  document.body.classList.remove('has-error');
  document.querySelector('#error-msg').textContent = '';
};

/**
 * @type {cca.App} Singleton of the Camera object.
 * @private
 */
cca.App.instance_ = null;

/**
 * Creates the Camera object and starts screen capturing.
 */
document.addEventListener('DOMContentLoaded', () => {
  var appWindow = chrome.app.window.current();
  if (!cca.App.instance_) {
    var inner = appWindow.innerBounds;
    cca.App.instance_ = new cca.App(inner.width / inner.height);
  }
  cca.App.instance_.start();
  appWindow.show();
});
