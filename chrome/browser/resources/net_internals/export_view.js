// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays options for exporting the captured data.
 *  @constructor
 */
function ExportView() {
  const mainBoxId = 'exportTabContent';
  const downloadIframeId = 'dataViewDownloadIframe';
  const saveFileButtonId = 'dataViewSaveLogFile';
  const dataViewSaveStatusTextId = 'dataViewSaveStatusText';
  const securityStrippingCheckboxId = 'securityStrippingCheckbox';

  DivView.call(this, mainBoxId);

  var securityStrippingCheckbox = $(securityStrippingCheckboxId);
  securityStrippingCheckbox.onclick =
      this.onSetSecurityStripping_.bind(this, securityStrippingCheckbox);

  this.downloadIframe_ = $(downloadIframeId);

  this.saveFileButton_ = $(saveFileButtonId);
  this.saveFileButton_.onclick = this.onSaveFile_.bind(this);
  this.saveStatusText_ = $(dataViewSaveStatusTextId);


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
  // Clean up previous blob, if any, to reduce resource usage.
  if (this.lastBlobURL_) {
    window.webkitURL.revokeObjectURL(this.lastBlobURL_);
    this.lastBlobURL_ = null;
  }
  this.setSaveFileStatus('Preparing data...', true);

  createLogDumpAsync(this.onLogDumpCreated_.bind(this));
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
