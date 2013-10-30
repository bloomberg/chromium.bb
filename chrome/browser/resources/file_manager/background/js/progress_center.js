// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Progress center at the background page.
 * @constructor
 */
var ProgressCenter = function() {
  cr.EventTarget.call(this);

  /**
   * Default container.
   * @type {ProgressItemContainer}
   * @private
   */
  this.targetContainer_ = ProgressItemContainer.CLIENT;

  /**
   * Current items managed by the progress center.
   * @type {Array.<ProgressItem>}
   * @private
   */
  this.items_ = [];

  /**
   * Timeout callback to remove items.
   * @type {TimeoutManager}
   * @private
   */
  this.resetTimeout_ = new ProgressCenter.TimeoutManager(
      this.reset_.bind(this));
};

/**
 * The default amount of milliseconds time, before a progress item will reset
 * after the last complete.
 * @type {number}
 * @private
 * @const
 */
ProgressCenter.RESET_DELAY_TIME_MS_ = 5000;

/**
 * Utility for timeout callback.
 *
 * @param {function(*):*} callback Callbakc function.
 * @constructor
 */
ProgressCenter.TimeoutManager = function(callback) {
  this.callback_ = callback;
  this.id_ = null;
  Object.seal(this);
};

/**
 * Requests timeout. Previous request is canceled.
 * @param {number} milliseconds Time to invoke the callback function.
 */
ProgressCenter.TimeoutManager.prototype.request = function(milliseconds) {
  if (this.id_)
    clearTimeout(this.id_);
  this.id_ = setTimeout(function() {
    this.id_ = null;
    this.callback_();
  }.bind(this), milliseconds);
};

ProgressCenter.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * Obtains the items to be displayed in the application window.
   * @private
   */
  get applicationItems() {
    return this.items_.filter(function(item) {
      return item.container == ProgressItemContainer.CLIENT;
    });
  }
};

/**
 * Updates the item in the progress center.
 * If the item has a new ID, the item is added to the item list.
 *
 * @param {ProgressCenterItem} item Updated item.
 */
ProgressCenter.prototype.updateItem = function(item) {
  var index = this.getItemIndex_(item.id);
  if (index === -1) {
    item.container = this.targetContainer_;
    this.items_.push(item);
  } else {
    this.items_[index] = item;
  }

  if (item.status !== ProgressItemState.PROGRESSING)
    this.resetTimeout_.request(ProgressCenter.RESET_DELAY_TIME_MS_);

  var event = new Event(ProgressCenterEvent.ITEM_UPDATED);
  event.item = item;
  this.dispatchEvent(event);
};

/**
 * Requests to cancel the progress item.
 * @param {string} id Progress ID to be requested to cancel.
 */
ProgressCenter.prototype.requestCancel = function(id) {
  var item = this.getItemById(id);
  if (item && item.cancelCallback)
    item.cancelCallback();
};

/**
 * Switches the default container.
 * @param {ProgressItemContainer} newContainer New value of the default
 *     container.
 */
ProgressCenter.prototype.switchContainer = function(newContainer) {
  if (this.targetContainer_ === newContainer)
    return;

  // Current items to be moved to the notification center.
  if (newContainer == ProgressItemContainer.NOTIFICATION) {
    var items = this.applicationItems;
    for (var i = 0; i < items.length; i++) {
      items[i].container = ProgressItemContainer.NOTIFICATION;
      this.postItemToNotification_(items);
    }
  }

  // The items in the notification center does not come back to the Files.app
  // clients.

  // Assign the new value.
  this.targetContainer_ = newContainer;
};

/**
 * Obtains the summarized item to be displayed in the closed progress center
 * panel.
 * @return {ProgressCenterItem} Summarized item. Returns null if there is no
 *     item.
 */
ProgressCenter.prototype.getSummarizedItem = function() {
  var applicationItems = this.applicationItems;
  if (applicationItems.length == 0)
    return null;
  if (applicationItems.length == 1)
    return applicationItems[0];
  var summarizedItem = new ProgressCenterItem();
  summarizedItem.summarized = true;
  var completeCount = 0;
  var progressingCount = 0;
  var canceledCount = 0;
  var errorCount = 0;
  for (var i = 0; i < applicationItems.length; i++) {
    switch (applicationItems[i].state) {
      case ProgressItemState.COMPLETE:
        completeCount++;
        break;
      case ProgressItemState.PROGRESSING:
        progressingCount++;
        break;
      case ProgressItemState.ERROR:
        errorCount++;
        continue;
      case ProgressItemState.CANCELED:
        canceledCount++;
        continue;
    }
    summarizedItem.progressMax += applicationItems[i].progressMax;
    summarizedItem.progressValue += applicationItems[i].progressValue;
  }
  var messages = [];
  if (completeCount)
    messages.push(completeCount + ' complete');
  if (progressingCount)
    messages.push(progressingCount + ' active');
  if (canceledCount)
    messages.push(canceledCount + ' canceled');
  if (errorCount)
    messages.push(errorCount + ' error');
  summarizedItem.message = messages.join(', ') + '.';
  summarizedItem.state =
      completeCount + progressingCount == 0 ? ProgressItemState.CANCELED :
      progressingCount > 0 ? ProgressItemState.PROGRESSING :
      ProgressItemState.COMPLETE;
  return summarizedItem;
};

/**
 * Obtains item by ID.
 * @param {string} id ID of progress item.
 * @return {ProgressCenterItem} Progress center item having the specified
 *     ID. Null if the item is not found.
 */
ProgressCenter.prototype.getItemById = function(id) {
  return this.items_[this.getItemIndex_(id)];
};

/**
 * Obtains item index that have the specifying ID.
 * @param {string} id Item ID.
 * @return {number} Item index. Returns -1 If the item is not found.
 * @private
 */
ProgressCenter.prototype.getItemIndex_ = function(id) {
  for (var i = 0; i < this.items_.length; i++) {
    if (this.items_[i].id === id)
      return i;
  }
  return -1;
};

/**
 * Passes the item to the ChromeOS's message center.
 *
 * TODO(hirono): Implement the method.
 *
 * @private
 */
ProgressCenter.prototype.passItemsToNotification_ = function() {

};

/**
 * Hides the progress center if there is no progressing items.
 * @private
 */
ProgressCenter.prototype.reset_ = function() {
  // If we have a progressing item, stop reset.
  for (var i = 0; i < this.items_.length; i++) {
    if (this.items_[i].state == ProgressItemState.PROGRESSING)
      return;
  }

  // Reset items.
  this.items_.splice(0, this.items_.length);

  // Dispatch a event.
  this.dispatchEvent(new Event(ProgressCenterEvent.RESET));
};

/**
 * An event handler for progress center.
 * @param {FileOperationManager} fileOperationManager File operation manager.
 * @param {ProgressCenter} progressCenter Progress center.
 * @constructor
 */
var ProgressCenterHandler = function(fileOperationManager, progressCenter) {
  /**
   * Number of deleted files.
   * @type {number}
   * @private
   */
  this.totalDeleted_ = 0;

  /**
   * File operation manager.
   * @type {FileOperationManager}
   * @private
   */
  this.fileOperationManager_ = fileOperationManager;

  /**
   * Progress center.
   * @type {progressCenter}
   * @private
   */
  this.progressCenter_ = progressCenter;

  // Seal the object.
  Object.seal(this);

  // Register event.
  fileOperationManager.addEventListener('copy-progress',
                                        this.onCopyProgress_.bind(this));
  fileOperationManager.addEventListener('delete',
                                        this.onDeleteProgress_.bind(this));
};

/**
 * Generate a progress message from the event.
 * @param {Event} event Progress event.
 * @return {string} message.
 * @private
 */
ProgressCenterHandler.getMessage_ = function(event) {
  if (event.reason === 'ERROR') {
    switch (event.error.code) {
      case util.FileOperationErrorType.TARGET_EXISTS:
        var name = event.error.data.name;
        if (event.error.data.isDirectory)
          name += '/';
        switch (event.status.operationType) {
          case 'COPY': return strf('COPY_TARGET_EXISTS_ERROR', name);
          case 'MOVE': return strf('MOVE_TARGET_EXISTS_ERROR', name);
          case 'ZIP': return strf('ZIP_TARGET_EXISTS_ERROR', name);
          default: return strf('TRANSFER_TARGET_EXISTS_ERROR', name);
        }

      case util.FileOperationErrorType.FILESYSTEM_ERROR:
        var detail = util.getFileErrorString(event.error.data.code);
        switch (event.status.operationType) {
          case 'COPY': return strf('COPY_FILESYSTEM_ERROR', detail);
          case 'MOVE': return strf('MOVE_FILESYSTEM_ERROR', detail);
          case 'ZIP': return strf('ZIP_FILESYSTEM_ERROR', detail);
          default: return strf('TRANSFER_FILESYSTEM_ERROR', detail);
        }

      default:
        switch (event.status.operationType) {
          case 'COPY': return strf('COPY_UNEXPECTED_ERROR', event.error);
          case 'MOVE': return strf('MOVE_UNEXPECTED_ERROR', event.error);
          case 'ZIP': return strf('ZIP_UNEXPECTED_ERROR', event.error);
          default: return strf('TRANSFER_UNEXPECTED_ERROR', event.error);
        }
    }
  } else if (event.status.numRemainingItems === 1) {
    var name = event.status.processingEntry.name;
    switch (event.status.operationType) {
      case 'COPY': return strf('COPY_FILE_NAME', name);
      case 'MOVE': return strf('MOVE_FILE_NAME', name);
      case 'ZIP': return strf('ZIP_FILE_NAME', name);
      default: return strf('TRANSFER_FILE_NAME', name);
    }
  } else {
    var remainNumber = event.status.numRemainingItems;
    switch (event.status.operationType) {
      case 'COPY': return strf('COPY_ITEMS_REMAINING', remainNumber);
      case 'MOVE': return strf('MOVE_ITEMS_REMAINING', remainNumber);
      case 'ZIP': return strf('ZIP_ITEMS_REMAINING', remainNumber);
      default: return strf('TRANSFER_ITEMS_REMAINING', remainNumber);
    }
  }
};

/**
 * Generate a delete message from the event.
 * @param {Event} event Progress event.
 * @param {number} totalDeleted Total number of deleted files.
 * @return {string} message.
 * @private
 */
ProgressCenterHandler.getDeleteMessage_ = function(event, totalDeleted) {
  if (totalDeleted === 1) {
    var fullPath = util.extractFilePath(event.urls[0]);
    var fileName = PathUtil.split(fullPath).pop();
    return strf('DELETED_MESSAGE', fileName);
  } else {
    return strf('DELETED_MESSAGE_PLURAL', totalDeleted);
  }
};

/**
 * Handles the copy-progress event.
 * @param {Event} event The copy-progress event.
 * @private
 */
ProgressCenterHandler.prototype.onCopyProgress_ = function(event) {
  var progressCenter = this.progressCenter_;
  var item;
  switch (event.reason) {
    case 'BEGIN':
      item = new ProgressCenterItem();
      item.id = event.taskId;
      item.message = ProgressCenterHandler.getMessage_(event);
      item.progressMax = event.status.totalBytes;
      item.progressValue = event.status.processedBytes;
      item.cancelCallback = this.fileOperationManager_.requestTaskCancel.bind(
          this.fileOperationManager_,
          event.taskId);
      progressCenter.updateItem(item);
      break;

    case 'PROGRESS':
      item = progressCenter.getItemById(event.taskId);
      if (!item) {
        console.error('Cannot find copying item.');
        return;
      }
      item.message = ProgressCenterHandler.getMessage_(event);
      item.progressValue = event.status.processedBytes;
      progressCenter.updateItem(item);
      break;

    case 'SUCCESS':
    case 'CANCELED':
    case 'ERROR':
      item = progressCenter.getItemById(event.taskId);
      if (!item) {
        // ERROR events can be dispatched before BEGIN events.
        item = new ProgressCenterItem();
        item.id = event.taskId;
        item.progressMax = 1;
      }
      if (event.reason === 'SUCCESS') {
        // TODO(hirono): Add a message for complete.
        item.state = ProgressItemState.COMPLETE;
        item.progressValue = item.progressMax;
      } else if (event.reason === 'CANCELED') {
        item.message = strf('COPY_CANCELLED');
        item.state = ProgressItemState.CANCELED;
      } else {
        item.message = ProgressCenterHandler.getMessage_(event);
        item.state = ProgressItemState.ERROR;
      }
      progressCenter.updateItem(item);
      break;
  }
};

/**
 * Handles the delete event.
 * @param {Event} event The delete event.
 * @private
 */
ProgressCenterHandler.prototype.onDeleteProgress_ = function(event) {
  var progressCenter = this.progressCenter_;
  var item;
  switch (event.reason) {
    case 'BEGIN':
      this.totalDeleted_ = 0;
      item = new ProgressCenterItem();
      item.id = event.taskId;
      // TODO(hirono): Specifying the correct message.
      item.message =
          ProgressCenterHandler.getDeleteMessage_(event, this.totalDeleted_);
      item.progressMax = 100;
      // TODO(hirono): Specify the cancel handler to the item.
      progressCenter.updateItem(item);
      break;

    case 'PROGRESS':
      item = progressCenter.getItemById(event.taskId);
      if (!item) {
        console.error('Cannot find deleting item.');
        return;
      }
      this.totalDeleted_ += event.urls.length;
      item.message =
          ProgressCenterHandler.getDeleteMessage_(event, this.totalDeleted_);
      progressCenter.updateItem(item);
      break;

    case 'SUCCESS':
    case 'ERROR':
      item = progressCenter.getItemById(event.taskId);
      if (!item) {
        console.error('Cannot find deleting item.');
        return;
      }
      if (event.reason === 'SUCCESS') {
        this.totalDeleted_ += event.urls.length;
        item.message =
            ProgressCenterHandler.getDeleteMessage_(event, this.totalDeleted_);
        item.state = ProgressItemState.COMPLETE;
        item.progressValue = item.progressMax;
      } else {
        item.message = str('DELETE_ERROR');
        item.state = ProgressItemState.ERROR;
      }
      progressCenter.updateItem(item);
      break;
  }
};
