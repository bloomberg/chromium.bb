// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * ImportingDialog manages the import process (which is really a copying).
 * @param {HTMLElement} parentNode Node to be parent for this dialog.
 * @param {FileCopyManager} copyManager Copy manager isntance.
 * @param {MetadataCache} metadataCache Metadata cache.
 * @constructor
 */
function ImportingDialog(parentNode, copyManager, metadataCache) {
  cr.ui.dialogs.BaseDialog.call(this, parentNode);
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
  this.frame_.textContent = '';

  this.imageBox_ = util.createChild(this.frame_, 'img-container');
  this.imageBox_.setAttribute('state', 'progress');

  var progressContainer = util.createChild(this.frame_, 'progress-container');
  progressContainer.appendChild(this.text_);

  this.progress_ = util.createChild(progressContainer, 'progress-bar');
  util.createChild(this.progress_, 'progress-track');

  this.cancelButton_.textContent =
      loadTimeData.getString('PHOTO_IMPORT_CANCEL_BUTTON');
  this.frame_.appendChild(this.cancelButton_);

  this.okButton_.textContent =
      loadTimeData.getString('OK_LABEL');
};

/**
 * Shows dialog.
 * @param {Array.<FileEntry>} entries Entries to import.
 * @param {DirectoryEntry} dir Directory to import to.
 * @param {boolean} move Whether to move files instead of copying them.
 */
ImportingDialog.prototype.show = function(entries, dir, move) {
  var message = loadTimeData.getString('PHOTO_IMPORT_IMPORTING');
  cr.ui.dialogs.BaseDialog.prototype.show.call(this, message, null, null, null);

  this.error_ = false;
  this.entries_ = entries;
  this.progress_.querySelector('.progress-track').style.width = '0';

  this.copyManager_.addEventListener('copy-progress',
      this.onCopyProgressBound_);

  this.previewEntry_(0);

  var files = entries.map(function(e) { return e.fullPath }).join('\n');
  var operationInfo = {
    isCut: move ? 'true' : 'false',
    isOnGData: PathUtil.getRootType(entries[0].fullPath) == RootType.GDATA,
    sourceDir: null,
    directories: '',
    files: files
  };
  this.copyManager_.paste(operationInfo, dir.fullPath, true);
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
 */
ImportingDialog.prototype.hide = function() {
  this.copyManager_.removeEventListener('copy-progress',
      this.onCopyProgressBound_);
  cr.ui.dialogs.BaseDialog.prototype.hide.call(this);
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
      this.frame_.removeChild(this.cancelButton_);
      this.frame_.appendChild(this.okButton_);
      break;

    case 'CANCELLED':
      this.hide();
      break;

    case 'ERROR':
      this.error_ = true;
      this.text_.textContent =
          loadTimeData.getString('PHOTO_IMPORT_IMPORTING_ERROR');
      this.imageBox_.setAttribute('state', 'error');
      this.frame_.removeChild(this.cancelButton_);
      this.frame_.appendChild(this.okButton_);
      break;

    default:
      console.error('Unknown "copy-progress" event reason: ' + event.reason);
  }
};
