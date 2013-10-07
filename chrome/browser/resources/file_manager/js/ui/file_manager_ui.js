// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * The root of the file manager's view managing the DOM of Files.app.
 *
 * @param {HTMLElement} element Top level element of Files.app.
 * @param {DialogType} dialogType Dialog type.
 * @constructor.
 */
var FileManagerUI = function(element, dialogType) {
  /**
   * Top level element of Files.app.
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * Dialog type.
   * @type {DialogType}
   * @private
   */
  this.dialogType_ = dialogType;

  /**
   * Error dialog.
   * @type {ErrorDialog}
   */
  this.errorDialog = null;

  /**
   * Alert dialog.
   * @type {cr.ui.dialogs.AlertDialog}
   */
  this.alertDialog = null;

  /**
   * Confirm dialog.
   * @type {cr.ui.dialogs.ConfirmDialog}
   */
  this.confirmDialog = null;

  /**
   * Prompt dialog.
   * @type {cr.ui.dialogs.PromptDialog}
   */
  this.promptDialog = null;

  /**
   * Share dialog.
   * @type {ShareDialog}
   */
  this.shareDialog = null;

  /**
   * Default task picker.
   * @type {DefaultActionDialog}
   */
  this.defaultTaskPicker = null;

  /**
   * Suggest apps dialog.
   * @type {SuggestAppsDialog}
   */
  this.suggestAppsDialog = null;

  Object.seal(this);
};

/**
 * Initializes the dialogs.
 */
FileManagerUI.prototype.initDialogs = function() {
  // Initialize the dialog label.
  var dialogs = cr.ui.dialogs;
  dialogs.BaseDialog.OK_LABEL = str('OK_LABEL');
  dialogs.BaseDialog.CANCEL_LABEL = str('CANCEL_LABEL');
  var appState = window.appState || {};

  // Create the dialog instances.
  this.errorDialog = new ErrorDialog(this.element_);
  this.alertDialog = new dialogs.AlertDialog(this.element_);
  this.confirmDialog = new dialogs.ConfirmDialog(this.element_);
  this.promptDialog = new dialogs.PromptDialog(this.element_);
  this.shareDialog = new ShareDialog(this.element_);
  this.defaultTaskPicker =
      new cr.filebrowser.DefaultActionDialog(this.element_);
  this.suggestAppsDialog = new SuggestAppsDialog(
      this.element_, appState.suggestAppsDialogState || {});
};

/**
 * Initializes the window buttons.
 */
FileManagerUI.prototype.initWindowButtons = function() {
  // Do not maximize/close when running via chrome://files in a browser.
  if (this.dialogType_ !== DialogType.FULL_PAGE ||
      util.platform.runningInBrowser())
    return;

  // This is to prevent the buttons from stealing focus on mouse down.
  var preventFocus = function(event) {
    event.preventDefault();
  };

  var maximizeButton = this.element_.querySelector('#maximize-button');
  maximizeButton.addEventListener('click', this.onMaximize_.bind(this));
  maximizeButton.addEventListener('mousedown', preventFocus);

  var closeButton = this.element_.querySelector('#close-button');
  closeButton.addEventListener('click', this.onClose_.bind(this));
  closeButton.addEventListener('mousedown', preventFocus);
};

/**
 * Handles maximize button click.
 * @private
 */
FileManagerUI.prototype.onMaximize_ = function() {
  var appWindow = chrome.app.window.current();
  if (appWindow.isMaximized())
    appWindow.restore();
  else
    appWindow.maximize();
};

/**
 * Hanldes close button click.
 * @private
 */
FileManagerUI.prototype.onClose_ = function() {
  // Do not close when running via chrome://files in a browser.
  if (util.platform.runningInBrowser())
    return;

  window.close();
};
