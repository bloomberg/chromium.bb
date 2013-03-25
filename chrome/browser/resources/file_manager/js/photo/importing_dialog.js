// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * ImportingDialog manages the import process (which is really a copying).
 * @param {HTMLElement} parentNode Node to be parent for this dialog.
 * @param {FileCopyManager} copyManager Copy manager isntance.
 * @param {MetadataCache} metadataCache Metadata cache.
 * @param {number=} opt_parentWindowId Id of the parent window.
 * @constructor
 */
function ImportingDialog(
    parentNode, copyManager, metadataCache, opt_parentWindowId) {
  cr.ui.dialogs.BaseDialog.call(this, parentNode);
  this.parentWindowId_ = opt_parentWindowId;
  this.copyManager_ = copyManager;
  this.metadataCache_ = metadataCache;
  this.onCopyProgressBound_ = this.onCopyProgress_.bind(this);
}

ImportingDialog.prototype = {
  __proto__: cr.ui.dialogs.BaseDialog.prototype
};

/**
 * One-time initialization of DOM.
 * @private
 */
ImportingDialog.prototype.initDom_ = function() {
  cr.ui.dialogs.BaseDialog.prototype.initDom_.call(this);

  this.container_.classList.add('importing-dialog');

  this.content_ = util.createChild(this.frame_, 'content');
  this.frame_.insertBefore(this.content_, this.frame_.firstChild);
  this.content_.appendChild(this.text_);

  this.imageBox_ = util.createChild(this.frame_, 'img-container');
  this.imageBox_.setAttribute('state', 'progress');
  this.frame_.insertBefore(this.imageBox_, this.frame_.firstChild);

  var progressContainer = util.createChild(this.content_, 'progress-container');
  this.progress_ = util.createChild(progressContainer, 'progress-bar');
  util.createChild(this.progress_, 'progress-track smoothed');

  this.buttons_ = this.frame_.querySelector('.cr-dialog-buttons');
  this.content_.appendChild(this.buttons_);

  this.viewButton_ = util.createChild(
      this.buttons_, 'cr-dialog-view', 'button');
  this.viewButton_.addEventListener('click',
                                    this.onViewClick_.bind(this));
  this.buttons_.insertBefore(this.viewButton_, this.buttons_.firstChild);

  this.viewButton_.textContent =
      loadTimeData.getString('VIEW_LABEL');
  this.viewButton_.hidden = true;
  this.cancelButton_.textContent =
      loadTimeData.getString('PHOTO_IMPORT_CANCEL_BUTTON');
  this.okButton_.textContent =
      loadTimeData.getString('OK_LABEL');
  this.okButton_.hidden = true;
};

/**
 * Shows dialog.
 * @param {Array.<FileEntry>} entries Entries to import.
 * @param {boolean} move Whether to move files instead of copying them.
 */
ImportingDialog.prototype.show = function(entries, move) {
  var message = loadTimeData.getString('PHOTO_IMPORT_IMPORTING');
  cr.ui.dialogs.BaseDialog.prototype.show.call(this, message, null, null, null);

  this.error_ = false;
  this.entries_ = entries;
  this.move_ = move;

  this.progress_.querySelector('.progress-track').style.width = '0';
  this.copyManager_.addEventListener('copy-progress',
      this.onCopyProgressBound_);

  this.previewEntry_(0);
};

/**
 * Starts copying.
 * @param {DirectoryEntry} destination Directory to import to.
 */
ImportingDialog.prototype.start = function(destination) {
  this.destination_ = destination;
  var files = this.entries_.map(function(e) { return e.fullPath }).join('\n');
  var operationInfo = {
    isCut: this.move_ ? 'true' : 'false',
    isOnDrive:
        PathUtil.getRootType(this.entries_[0].fullPath) == RootType.DRIVE,
    sourceDir: null,
    directories: '',
    files: files
  };
  this.copyManager_.paste(operationInfo, this.destination_.fullPath, true);
};

/**
 * Shows entry preview.
 * @param {number} index Entry index.
 * @private
 */
ImportingDialog.prototype.previewEntry_ = function(index) {
  var box = this.imageBox_;
  var entry = this.entries_[index];
  this.metadataCache_.get(entry, 'thumbnail|filesystem',
      function(metadata) {
        new ThumbnailLoader(entry.toURL(),
                            ThumbnailLoader.LoaderType.IMAGE,
                            metadata).
            load(box, ThumbnailLoader.FillMode.FILL);
      });
};

/**
 * Closes dialog.
 * @param {function()=} opt_onHide Completion callback.
 */
ImportingDialog.prototype.hide = function(opt_onHide) {
  this.copyManager_.removeEventListener('copy-progress',
      this.onCopyProgressBound_);
  cr.ui.dialogs.BaseDialog.prototype.hide.call(this, opt_onHide);
};

/**
 * Cancel button click event handler.
 * @private
 */
ImportingDialog.prototype.onCancelClick_ = function() {
  this.copyManager_.requestCancel();
  this.hide();
};

/**
 * OK button click event handler.
 * @private
 */
ImportingDialog.prototype.onOkClick_ = function() {
  this.hide();
  if (!this.error_)
    window.close();
};

/**
 * View button click event handler. Invokes the mosaic view.
 * @private
 */
ImportingDialog.prototype.onViewClick_ = function() {
  var url = util.platform.getURL('main.html') +
      '?{%22gallery%22:%22mosaic%22}#' + this.destination_.fullPath;
  if (!this.parentWindowId_ ||
      !util.redirectMainWindow(this.parentWindowId_, url)) {
    // The parent window hasn't been found. Launch the url as a new window.
    // TODO(mtomasz): Change it to chrome.fileBrowserPrivate.openNewWindow.
    util.platform.createWindow(url);
  }
  this.hide();
  window.close();
};

/**
 * Event handler for keydown event.
 * @param {Event} event The event.
 * @private
 */
ImportingDialog.prototype.onContainerKeyDown_ = function(event) {
  // Handle Escape.
  if (event.keyCode == 27) {
    this.onCancelClick_(event);
    event.preventDefault();
  }
};

/**
 * 'copy-progress' event handler. Show progress.
 * @param {cr.Event} event A 'copy-progress' event from FileCopyManager.
 * @private
 */
ImportingDialog.prototype.onCopyProgress_ = function(event) {
  switch (event.reason) {
    case 'BEGIN':
    case 'PROGRESS':
      var progress = this.copyManager_.getStatus().percentage;
      this.progress_.querySelector('.progress-track').style.width =
          (progress * 100) + '%';
      var index = Math.round(this.entries_.length * progress);
      if (index == this.entries_.length) index--;
      this.previewEntry_(index);
      break;

    case 'SUCCESS':
      this.text_.textContent =
          loadTimeData.getString('PHOTO_IMPORT_IMPORT_COMPLETE');
      this.imageBox_.setAttribute('state', 'success');
      this.cancelButton_.hidden = true;
      this.viewButton_.hidden = false;
      this.okButton_.hidden = false;
      break;

    case 'CANCELLED':
      this.hide();
      break;

    case 'ERROR':
      this.error_ = true;
      this.text_.textContent =
          loadTimeData.getString('PHOTO_IMPORT_IMPORTING_ERROR');
      this.imageBox_.setAttribute('state', 'error');
      this.cancelButton_.hidden = true;
      this.viewButton_.hidden = true;
      this.okButton_.hidden = false;
      break;

    default:
      console.error('Unknown "copy-progress" event reason: ' + event.reason);
  }
};
