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
