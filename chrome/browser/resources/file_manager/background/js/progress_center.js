// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Progress center at the background page.
 * @constructor
 */
var ProgressCenter = function() {
  /**
   * Current items managed by the progress center.
   * @type {Array.<ProgressItem>}
   * @private
   */
  this.items_ = [];

  /**
   * Map of progress ID and notification ID.
   * @type {Object.<string, string>}
   * @private
   */
  this.notifications_ = new ProgressCenter.Notifications_(
      this.requestCancel.bind(this));

  /**
   * List of panel UI managed by the progress center.
   * @type {Array.<ProgressCenterPanel>}
   * @private
   */
  this.panels_ = [];

  /**
   * Timeout callback to remove items.
   * @type {ProgressCenter.TimeoutManager_}
   * @private
   */
  this.resetTimeout_ = new ProgressCenter.TimeoutManager_(
      this.reset_.bind(this));

  Object.seal(this);
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
 * Notifications created by progress center.
 * @param {function(string)} cancelCallback Callback to notify the progress
 *     center of cancel operation.
 * @constructor
 * @private
 */
ProgressCenter.Notifications_ = function(cancelCallback) {
  /**
   * ID set of notifications that is progressing now.
   * @type {Object.<string, ProgressCenter.Notifications_.NotificationState_>}
   * @private
   */
  this.ids_ = {};

  /**
   * Async queue.
   * @type {AsyncUtil.Queue}
   * @private
   */
  this.queue_ = new AsyncUtil.Queue();

  /**
   * Callback to notify the progress center of cancel operation.
   * @type {function(string)}
   * @private
   */
  this.cancelCallback_ = cancelCallback;

  chrome.notifications.onButtonClicked.addListener(
      this.onButtonClicked_.bind(this));
  chrome.notifications.onClosed.addListener(this.onClosed_.bind(this));

  Object.seal(this);
};

/**
 * State of notification.
 * @enum {string}
 * @const
 * @private
 */
ProgressCenter.Notifications_.NotificationState_ = Object.freeze({
  VISIBLE: 'visible',
  DISMISSED: 'dismissed'
});

/**
 * Updates the notification according to the item.
 * @param {ProgressCenterItem} item Item to contain new information.
 * @param {boolean} newItemAcceptable Whether to accept new item or not.
 */
ProgressCenter.Notifications_.prototype.updateItem = function(
    item, newItemAcceptable) {
  var NotificationState = ProgressCenter.Notifications_.NotificationState_;
  var newlyAdded = !(item.id in this.ids_);

  // If new item is not acceptable, just return.
  if (newlyAdded && !newItemAcceptable)
    return;

  // Update the ID map and return if we does not show a notification for the
  // item.
  if (item.state === ProgressItemState.PROGRESSING) {
    if (newlyAdded)
      this.ids_[item.id] = NotificationState.VISIBLE;
    else if (this.ids_[item.id] === NotificationState.DISMISSED)
      return;
  } else {
    // This notification is no longer tracked.
    var previousState = this.ids_[item.id];
    delete this.ids_[item.id];
    // Clear notifications for complete or canceled items.
    if (item.state === ProgressItemState.CANCELED ||
        item.state === ProgressItemState.COMPLETED) {
      if (previousState === NotificationState.VISIBLE) {
        this.queue_.run(function(proceed) {
          chrome.notifications.clear(item.id, proceed);
        });
      }
      return;
    }
  }

  // Create/update the notification with the item.
  this.queue_.run(function(proceed) {
    var params = {
      title: chrome.runtime.getManifest().name,
      iconUrl: chrome.runtime.getURL('/common/images/icon96.png'),
      type: item.state === ProgressItemState.PROGRESSING ? 'progress' : 'basic',
      message: item.message,
      buttons: item.cancelable ? [{title: str('CANCEL_LABEL')}] : undefined,
      progress: item.state === ProgressItemState.PROGRESSING ?
          item.progressRateByPercent : undefined
    };
    if (newlyAdded)
      chrome.notifications.create(item.id, params, proceed);
    else
      chrome.notifications.update(item.id, params, proceed);
  }.bind(this));
};

/**
 * Handles cancel button click.
 * @param {string} id Item ID.
 * @private
 */
ProgressCenter.Notifications_.prototype.onButtonClicked_ = function(id) {
  if (id in this.ids_)
    this.cancelCallback_(id);
};

/**
 * Handles notification close.
 * @param {string} id Item ID.
 * @private
 */
ProgressCenter.Notifications_.prototype.onClosed_ = function(id) {
  if (id in this.ids_)
    this.ids_[id] = ProgressCenter.Notifications_.NotificationState_.DISMISSED;
};

/**
 * Utility for timeout callback.
 *
 * @param {function(*):*} callback Callback function.
 * @constructor
 * @private
 */
ProgressCenter.TimeoutManager_ = function(callback) {
  this.callback_ = callback;
  this.id_ = null;
  Object.seal(this);
};

/**
 * Requests timeout. Previous request is canceled.
 * @param {number} milliseconds Time to invoke the callback function.
 */
ProgressCenter.TimeoutManager_.prototype.request = function(milliseconds) {
  if (this.id_)
    clearTimeout(this.id_);
  this.id_ = setTimeout(function() {
    this.id_ = null;
    this.callback_();
  }.bind(this), milliseconds);
};

/**
 * Cancels the timeout and invoke the callback function synchronously.
 */
ProgressCenter.TimeoutManager_.prototype.callImmediately = function() {
  if (this.id_)
    clearTimeout(this.id_);
  this.id_ = null;
  this.callback_();
};

/**
 * Updates the item in the progress center.
 * If the item has a new ID, the item is added to the item list.
 *
 * @param {ProgressCenterItem} item Updated item.
 */
ProgressCenter.prototype.updateItem = function(item) {
  // Update item.
  var index = this.getItemIndex_(item.id);
  if (index === -1)
    this.items_.push(item);
  else
    this.items_[index] = item;

  // Update panels.
  var summarizedItem = this.getSummarizedItem_();
  for (var i = 0; i < this.panels_.length; i++) {
    this.panels_[i].updateItem(item);
    this.panels_[i].updateCloseView(summarizedItem);
  }

  // Update notifications.
  this.notifications_.updateItem(item, !this.panels_.length);

  // Reset if there is no item.
  for (var i = 0; i < this.items_.length; i++) {
    switch (this.items_[i].state) {
      case ProgressItemState.PROGRESSING:
        return;
      case ProgressItemState.ERROR:
        this.resetTimeout_.request(ProgressCenter.RESET_DELAY_TIME_MS_);
        return;
    }
  }

  // Cancel timeout and call reset function immediately.
  this.resetTimeout_.callImmediately();
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
 * Adds a panel UI to the notification center.
 * @param {ProgressCenterPanel} panel Panel UI.
 */
ProgressCenter.prototype.addPanel = function(panel) {
  if (this.panels_.indexOf(panel) !== -1)
    return;

  // Update the panel list.
  this.panels_.push(panel);

  // Set the current items.
  for (var i = 0; i < this.items_.length; i++)
    panel.updateItem(this.items_[i]);
  var summarizedItem = this.getSummarizedItem_();
  if (summarizedItem)
    panel.updateCloseView(summarizedItem);

  // Register the cancel callback.
  panel.cancelCallback = this.requestCancel.bind(this);
};

/**
 * Removes a panel UI from the notification center.
 * @param {ProgressCenterPanel} panel Panel UI.
 */
ProgressCenter.prototype.removePanel = function(panel) {
  var index = this.panels_.indexOf(panel);
  if (index === -1)
    return;

  this.panels_.splice(index, 1);
  panel.cancelCallback = null;

  // If there is no panel, show the notifications.
  if (this.panels_.length)
    return;
  for (var i = 0; i < this.items_.length; i++)
    this.notifications_.updateItem(this.items_[i], true);
};

/**
 * Obtains the summarized item to be displayed in the closed progress center
 * panel.
 * @return {ProgressCenterItem} Summarized item. Returns null if there is no
 *     item.
 * @private
 */
ProgressCenter.prototype.getSummarizedItem_ = function() {
  var summarizedItem = new ProgressCenterItem();
  var progressingItems = [];
  var completedItems = [];
  var canceledItems = [];
  var errorItems = [];

  for (var i = 0; i < this.items_.length; i++) {
    // Count states.
    switch (this.items_[i].state) {
      case ProgressItemState.PROGRESSING:
        progressingItems.push(this.items_[i]);
        break;
      case ProgressItemState.COMPLETED:
        completedItems.push(this.items_[i]);
        break;
      case ProgressItemState.CANCELED:
        canceledItems.push(this.items_[i]);
        break;
      case ProgressItemState.ERROR:
        errorItems.push(this.items_[i]);
        break;
    }

    // If all of the progressing items have the same type, then use
    // it. Otherwise use TRANSFER, since it is the most generic.
    if (this.items_[i].state === ProgressItemState.PROGRESSING) {
      if (summarizedItem.type === null)
        summarizedItem.type = this.items_[i].type;
      else if (summarizedItem.type !== this.items_[i].type)
        summarizedItem.type = ProgressItemType.TRANSFER;
    }

    // Sum up the progress values.
    if (this.items_[i].state === ProgressItemState.PROGRESSING ||
        this.items_[i].state === ProgressItemState.COMPLETED) {
      summarizedItem.progressMax += this.items_[i].progressMax;
      summarizedItem.progressValue += this.items_[i].progressValue;
    }
  }

  // If there are multiple visible (progressing and error) items, show the
  // summarized message.
  if (progressingItems.length + errorItems.length > 1) {
    // Set item message.
    var messages = [];
    if (progressingItems.length > 0) {
      switch (summarizedItem.type) {
        case ProgressItemType.COPY:
          messages.push(str('COPY_PROGRESS_SUMMARY'));
          break;
        case ProgressItemType.MOVE:
          messages.push(str('MOVE_PROGRESS_SUMMARY'));
          break;
        case ProgressItemType.DELETE:
          messages.push(str('DELETE_PROGRESS_SUMMARY'));
          break;
        case ProgressItemType.ZIP:
          messages.push(str('ZIP_PROGRESS_SUMMARY'));
          break;
        case ProgressItemType.TRANSFER:
          messages.push(str('TRANSFER_PROGRESS_SUMMARY'));
          break;
      }
    }
    if (errorItems.length === 1)
      messages.push(str('ERROR_PROGRESS_SUMMARY'));
    else if (errorItems.length > 1)
      messages.push(strf('ERROR_PROGRESS_SUMMARY_PLURAL', errorItems.length));

    summarizedItem.summarized = true;
    summarizedItem.message = messages.join(' ');
    summarizedItem.state = progressingItems.length > 0 ?
        ProgressItemState.PROGRESSING : ProgressItemState.ERROR;
    return summarizedItem;
  }

  // If there is 1 visible item, show the item message.
  if (progressingItems.length + errorItems.length === 1) {
    var visibleItem = progressingItems[0] || errorItems[0];
    summarizedItem.id = visibleItem.id;
    summarizedItem.cancelCallback = visibleItem.cancelCallback;
    summarizedItem.type = visibleItem.type;
    summarizedItem.message = visibleItem.message;
    summarizedItem.state = visibleItem.state;
    return summarizedItem;
  }

  // If there is no visible item, the message can be empty.
  if (completedItems.length > 0) {
    summarizedItem.state = ProgressItemState.COMPLETED;
    return summarizedItem;
  }
  if (canceledItems.length > 0) {
    summarizedItem.state = ProgressItemState.CANCELED;
    return summarizedItem;
  }

  // If there is no item, return null.
  return null;
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
  for (var i = 0; i < this.panels_.length; i++)
    this.panels_[i].reset();
};

/**
 * An event handler for progress center.
 * @param {FileOperationManager} fileOperationManager File operation manager.
 * @param {ProgressCenter} progressCenter Progress center.
 * @constructor
 */
var ProgressCenterHandler = function(fileOperationManager, progressCenter) {
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

  /**
   * Pending items of delete operation.
   *
   * Delete operations are usually complete quickly.
   * So we would not like to show the progress bar at first.
   * If the operation takes more than ProgressCenterHandler.PENDING_TIME_MS_,
   * we adds the item to the progress center.
   *
   * @type {Object.<string, ProgressCenterItem>}}
   * @private
   */
  this.pendingItems_ = {};

  // Register event.
  fileOperationManager.addEventListener('copy-progress',
                                        this.onCopyProgress_.bind(this));
  fileOperationManager.addEventListener('delete',
                                        this.onDeleteProgress_.bind(this));

  // Seal the object.
  Object.seal(this);
};

/**
 * Pending time before a delete item is added to the progress center.
 *
 * @type {number}
 * @const
 * @private
 */
ProgressCenterHandler.PENDING_TIME_MS_ = 500;

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
          case 'COPY': return strf('COPY_UNEXPECTED_ERROR', event.error.code);
          case 'MOVE': return strf('MOVE_UNEXPECTED_ERROR', event.error.code);
          case 'ZIP': return strf('ZIP_UNEXPECTED_ERROR', event.error.code);
          default: return strf('TRANSFER_UNEXPECTED_ERROR', event.error.code);
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
 * Generates a delete message from the event.
 * @param {Event} event Progress event.
 * @return {string} message.
 * @private
 */
ProgressCenterHandler.getDeleteMessage_ = function(event) {
  if (event.reason === 'ERROR') {
    return str('DELETE_ERROR');
  } else if (event.entries.length == 1) {
    var fileName = event.entries[0].name;
    return strf('DELETE_FILE_NAME', fileName);
  } else if (event.entries.length > 1) {
    return strf('DELETE_ITEMS_REMAINING', event.entries.length);
  } else {
    return '';
  }
};

/**
 * Obtains ProgressItemType from OperationType of FileTransferManager.
 * @param {string} operationType OperationType of FileTransferManager.
 * @return {ProgressItemType} ProgreeType corresponding to the specified
 *     operation type.
 * @private
 */
ProgressCenterHandler.getType_ = function(operationType) {
  switch (operationType) {
    case 'COPY': return ProgressItemType.COPY;
    case 'MOVE': return ProgressItemType.MOVE;
    case 'ZIP': return ProgressItemType.ZIP;
    default:
      console.error('Unknown operation type.');
      return ProgressItemType.TRANSFER;
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
      item.type = ProgressCenterHandler.getType_(event.status.operationType);
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
        item.type = ProgressCenterHandler.getType_(event.status.operationType);
        item.id = event.taskId;
        item.progressMax = 1;
      }
      if (event.reason === 'SUCCESS') {
        item.message = '';
        item.state = ProgressItemState.COMPLETED;
        item.progressValue = item.progressMax;
      } else if (event.reason === 'CANCELED') {
        item.message = '';
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
  var pending;
  switch (event.reason) {
    case 'BEGIN':
      item = new ProgressCenterItem();
      item.id = event.taskId;
      item.type = ProgressItemType.DELETE;
      item.message = ProgressCenterHandler.getDeleteMessage_(event);
      item.progressMax = event.totalBytes;
      item.progressValue = event.processedBytes;

      // TODO(hirono): Specify the cancel handler to the item.
      this.pendingItems_[item.id] = item;
      setTimeout(this.showPendingItem_.bind(this, item),
                 ProgressCenterHandler.PENDING_TIME_MS_);
      break;

    case 'PROGRESS':
      pending = event.taskId in this.pendingItems_;
      item = this.pendingItems_[event.taskId] ||
          progressCenter.getItemById(event.taskId);
      if (!item) {
        console.error('Cannot find deleting item.');
        return;
      }
      item.message = ProgressCenterHandler.getDeleteMessage_(event);
      item.progressMax = event.totalBytes;
      item.progressValue = event.processedBytes;
      if (!pending)
        progressCenter.updateItem(item);
      break;

    case 'SUCCESS':
    case 'ERROR':
      // Obtain working variable.
      pending = event.taskId in this.pendingItems_;
      item = this.pendingItems_[event.taskId] ||
          progressCenter.getItemById(event.taskId);
      if (!item) {
        console.error('Cannot find deleting item.');
        return;
      }

      // Update the item.
      item.message = ProgressCenterHandler.getDeleteMessage_(event);
      if (event.reason === 'SUCCESS') {
        item.state = ProgressItemState.COMPLETED;
        item.progressValue = item.progressMax;
      } else {
        item.state = ProgressItemState.ERROR;
      }

      // Apply the change.
      if (!pending || event.reason === 'ERROR')
        progressCenter.updateItem(item);
      if (pending)
        delete this.pendingItems_[event.taskId];
      break;
  }
};

/**
 * Shows the pending item.
 *
 * @param {ProgressCenterItem} item Pending item.
 * @private
 */
ProgressCenterHandler.prototype.showPendingItem_ = function(item) {
  // The item is already gone.
  if (!this.pendingItems_[item.id])
    return;
  delete this.pendingItems_[item.id];
  this.progressCenter_.updateItem(item);
};
