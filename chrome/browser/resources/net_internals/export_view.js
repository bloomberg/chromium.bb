// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays options for exporting the captured data.
 *  @constructor
 */
function ExportView() {
  const mainBoxId = 'export-view-tab-content';
  const downloadIframeId = 'export-view-download-iframe';
  const saveFileButtonId = 'export-view-save-log-file';
  const saveStatusTextId = 'export-view-save-status-text';
  const securityStrippingCheckboxId = 'export-view-security-stripping-checkbox';
  const userCommentsTextAreaId = "export-view-user-comments";

  DivView.call(this, mainBoxId);

  var securityStrippingCheckbox = $(securityStrippingCheckboxId);
  securityStrippingCheckbox.onclick =
      this.onSetSecurityStripping_.bind(this, securityStrippingCheckbox);

  this.downloadIframe_ = $(downloadIframeId);

  this.saveFileButton_ = $(saveFileButtonId);
  this.saveFileButton_.onclick = this.onSaveFile_.bind(this);
  this.saveStatusText_ = $(saveStatusTextId);

  this.userCommentsTextArea_ = $(userCommentsTextAreaId);

  // Track blob for previous log dump so it can be revoked when a new dump is
  // saved.
  this.lastBlobURL_ = null;
}

inherits(ExportView, DivView);

/**
 * Depending on the value of the checkbox, enables or disables stripping
 * cookies and passwords from log dumps and displayed events.
 */
ExportView.prototype.onSetSecurityStripping_ =
    function(securityStrippingCheckbox) {
  g_browser.sourceTracker.setSecurityStripping(
      securityStrippingCheckbox.checked);
};

ExportView.prototype.onSecurityStrippingChanged = function() {
};

/**
 * Called when a log file is loaded, after clearing the old log entries and
 * loading the new ones.  Returns false to indicate the view should be hidden.
 */
ExportView.prototype.onLoadLogFinish = function(data) {
  return false;
};

/**
 * Sets the save to file status text, displayed below the save to file button,
 * to |text|.  Also enables or disables the save button based on the value
 * of |isSaving|, which must be true if the save process is still ongoing, and
 * false when the operation has stopped, regardless of success of failure.
 */
ExportView.prototype.setSaveFileStatus = function(text, isSaving) {
  this.enableSaveFileButton_(!isSaving);
  this.saveStatusText_.textContent = text;
};

ExportView.prototype.enableSaveFileButton_ = function(enabled) {
  this.saveFileButton_.disabled = !enabled;
};

/**
 * If not already busy saving a log dump, triggers asynchronous
 * generation of log dump and starts waiting for it to complete.
 */
ExportView.prototype.onSaveFile_ = function() {
  if (this.saveFileButton_.disabled)
    return;

  // Get an explanation for the dump file (this is mandatory!)
  var userComments = this.getNonEmptyUserComments_();
  if (userComments == undefined) {
    return;
  }

  // Clean up previous blob, if any, to reduce resource usage.
  if (this.lastBlobURL_) {
    window.webkitURL.revokeObjectURL(this.lastBlobURL_);
    this.lastBlobURL_ = null;
  }
  this.setSaveFileStatus('Preparing data...', true);

  createLogDumpAsync(userComments, this.onLogDumpCreated_.bind(this));
};

/**
 * Fetches the user comments for this dump. If none were entered, warns the user
 * and returns undefined. Otherwise returns the comments text.
 */
ExportView.prototype.getNonEmptyUserComments_ = function() {
  var value = this.userCommentsTextArea_.value;

  // Reset the class name in case we had hilighted it earlier.
  this.userCommentsTextArea_.className = ''

  // We don't accept empty explanations. We don't care what is entered, as long
  // as there is something (a single whitespace would work).
  if (value == '') {
    // Put a big obnoxious red border around the text area.
    this.userCommentsTextArea_.className = 'export-view-explanation-warning';
    alert('Please fill in the text field!');
    return undefined;
  }

  return value;
};

/**
 * Creates a blob url and starts downloading it.
 */
ExportView.prototype.onLogDumpCreated_ = function(dumpText) {
  var blobBuilder = new WebKitBlobBuilder();
  blobBuilder.append(dumpText, 'native');
  var textBlob = blobBuilder.getBlob('octet/stream');
  this.lastBlobURL_ = window.webkitURL.createObjectURL(textBlob);
  this.downloadIframe_.src = this.lastBlobURL_;
  this.setSaveFileStatus('Dump successful', false);
};
