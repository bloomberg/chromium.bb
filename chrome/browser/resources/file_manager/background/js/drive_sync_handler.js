// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Handler of the background page for the drive sync events.
 * @param {ProgressCenter} progressCenter Progress center to submit the
 *     progressing items.
 * @constructor
 */
function DriveSyncHandler(progressCenter) {
  /**
   * Progress center to submit the progressing item.
   * @type {ProgressCenter}
   * @private
   */
  this.progressCenter_ = progressCenter;

  /**
   * Counter for progress ID.
   * @type {number}
   * @private
   */
  this.idCounter_ = 0;

  /**
   * Map of file urls and progress center items.
   * @type {Object.<string, ProgressCenterItem>}
   * @private
   */
  this.items_ = {};

  /**
   * Async queue.
   * @type {AsyncUtil.Queue}
   * @private
   */
  this.queue_ = new AsyncUtil.Queue();

  // Register events.
  chrome.fileBrowserPrivate.onFileTransfersUpdated.addListener(
      this.onFileTransfersUpdated_.bind(this));
  chrome.fileBrowserPrivate.onDriveSyncError.addListener(
      this.onDriveSyncError_.bind(this));
}

/**
 * Completed event name.
 * @type {string}
 * @const
 */
DriveSyncHandler.COMPLETED_EVENT = 'completed';

/**
 * Progress ID of the drive sync.
 * @type {string}
 * @const
 */
DriveSyncHandler.PROGRESS_ITEM_ID_PREFIX = 'drive-sync-';

DriveSyncHandler.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * @return {boolean} Whether the handler is having syncing items or not.
   */
  get syncing() {
    // Check if this.items_ has properties or not.
    for (var url in this.items_) {
      return true;
    }
    return false;
  }
};

/**
 * Handles file transfer updated events.
 * @param {Array.<FileTransferStatus>} statusList List of drive status.
 * @private
 */
DriveSyncHandler.prototype.onFileTransfersUpdated_ = function(statusList) {
  var completed = false;
  for (var i = 0; i < statusList.length; i++) {
    var status = statusList[i];
    switch (status.transferState) {
      case 'in_progress':
      case 'started':
        this.updateItem_(status);
        break;
      case 'completed':
      case 'failed':
        this.removeItem_(status);
        if (!this.syncing)
          this.dispatchEvent(new Event(DriveSyncHandler.COMPLETED_EVENT));
        break;
      default:
        throw new Error(
            'Invalid transfer state: ' + status.transferState + '.');
    }
  }
};

/**
 * Updates the item involved with the given status.
 * @param {FileTransferStatus} status Transfer status.
 * @private
 */
DriveSyncHandler.prototype.updateItem_ = function(status) {
  this.queue_.run(function(callback) {
    if (this.items_[status.fileUrl]) {
      callback();
      return;
    }
    webkitResolveLocalFileSystemURL(status.fileUrl, function(entry) {
      var item = new ProgressCenterItem();
      item.id =
          DriveSyncHandler.PROGRESS_ITEM_ID_PREFIX + (this.idCounter_++);
      item.type = ProgressItemType.SYNC;
      item.quiet = true;
      item.message = strf('SYNC_FILE_NAME', entry.name);
      item.cancelCallback = this.requestCancel_.bind(this, entry);
      this.items_[status.fileUrl] = item;
      callback();
    }.bind(this), function(error) {
      console.warn('Resolving URL ' + status.fileUrl + ' is failed: ', error);
      callback();
    });
  }.bind(this));
  this.queue_.run(function(callback) {
    var item = this.items_[status.fileUrl];
    if (!item) {
      callback();
      return;
    }
    item.progressValue = status.processed || 0;
    item.progressMax = status.total || 1;
    this.progressCenter_.updateItem(item);
    callback();
  }.bind(this));
};

/**
 * Removes the item involved with the given status.
 * @param {FileTransferStatus} status Transfer status.
 * @private
 */
DriveSyncHandler.prototype.removeItem_ = function(status) {
  this.queue_.run(function(callback) {
    var item = this.items_[status.fileUrl];
    if (!item) {
      callback();
      return;
    }
    item.state = status.transferState === 'completed' ?
        ProgressItemState.COMPLETED : ProgressItemState.CANCELED;
    this.progressCenter_.updateItem(item);
    delete this.items_[status.fileUrl];
    callback();
  }.bind(this));
};

/**
 * Requests to cancel for the given files' drive sync.
 * @param {Entry} entry Entry to be canceled.
 * @private
 */
DriveSyncHandler.prototype.requestCancel_ = function(entry) {
  chrome.fileBrowserPrivate.cancelFileTransfers([entry.toURL()], function() {});
};

/**
 * Handles drive's sync errors.
 * @param {DriveSyncErrorEvent} event Drive sync error event.
 * @private
 */
DriveSyncHandler.prototype.onDriveSyncError_ = function(event) {
  webkitResolveLocalFileSystemURL(event.fileUrl, function(entry) {
    var item;
    item = new ProgressCenterItem();
    item.id = DriveSyncHandler.PROGRESS_ITEM_ID_PREFIX + (this.idCounter_++);
    item.type = ProgressItemType.SYNC;
    item.quiet = true;
    item.state = ProgressItemState.ERROR;
    switch (event.type) {
      case 'delete_without_permission':
        item.message =
            strf('SYNC_DELETE_WITHOUT_PERMISSION_ERROR', entry.name);
        break;
      case 'service_unavailable':
        item.message = str('SYNC_SERVICE_UNAVAILABLE_ERROR');
        break;
      case 'misc':
        item.message = strf('SYNC_MISC_ERROR', entry.name);
        break;
    }
    this.progressCenter_.updateItem(item);
  }.bind(this));
};
