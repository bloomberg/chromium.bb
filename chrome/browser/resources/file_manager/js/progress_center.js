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
   * ID counter.
   * @type {number}
   * @private
   */
  this.idCounter_ = 1;

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
   * Async queue to operate the items array sequencially.
   * @type {AsyncUtil.Queue}
   * @private
   */
  this.queue_ = new AsyncUtil.Queue();
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
 * Adds an item to the progress center.
 * @param {ProgressItem} item Item to be added.
 */
ProgressCenter.prototype.addItem = function(item) {
  // If application window is opening, the item is displayed in the window.
  // Otherwise the item is displayed in nortification.
  item.id = this.idCounter_++;
  item.container = this.targetContainer_;
  this.items_.push(item);

  var event = new cr.Event(ProgressCenterEvent.ITEM_ADDED);
  event.item = item;
  this.dispatchEvent(event);
};

/**
 * Updates the item in the progress center.
 *
 * @param {ProgressCenterItem} item New contents of the item.
 */
ProgressCenter.prototype.updateItem = function(item) {
  var index = this.getItemIndex_(item.id);
  if (index === -1)
    return;
  this.items_[index] = item;

  var event = new cr.Event(ProgressCenterEvent.ITEM_UPDATED);
  event.item = item;
  this.dispatchEvent(event);
};

/**
 * Removes the item in the progress center.
 *
 * @param {number} id ID of the item to be removed.
 */
ProgressCenter.prototype.removeItem = function(id) {
  var index = this.getItemIndex_(id);
  if (index === -1)
    return;
  this.items_.splice(index, 1);

  var event = new cr.Event(ProgressCenterEvent.ITEM_REMOVED);
  event.itemId = id;
  this.dispatchEvent(event);
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
 * Obtains item index that have the specifing ID.
 * @param {number} id Item ID.
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
 * An event handler for progress center.
 * @param {FileOperationManager} fileOperationManager File operation manager.
 * @param {ProgressCenter} progressCenter Progress center.
 * @constructor
 */
var ProgressCenterHandler = function(fileOperationManager, progressCenter) {
  /**
   * Copying progress item.
   * @type {ProgressCenterItem}
   * @private
   */
  this.copyingItem_ = null;

  /**
   * Deleting progress item.
   * @type {ProgressCenterItem}
   * @private
   */
  this.deletingItem_ = null;

  // Seal the object.
  Object.seal(this);

  // Register event.
  fileOperationManager.addEventListener('copy-progress',
                                        this.onCopyProgress_.bind(this));
  fileOperationManager.addEventListener('delete',
                                        this.onDeleteProgress_.bind(this));
};

/**
 * Handles the copy-progress event.
 * @param {Event} event The copy-progress event.
 * @private
 */
ProgressCenterHandler.prototype.onCopyProgress_ = function(event) {
  switch (event.reason) {
    case 'BEGIN':
      if (this.copyingItem_) {
        console.error('Previous copy is not completed.');
        return;
      }
      this.copyingItem_ = new ProgressCenterItem();
      // TODO(hirono): Specifying the correct message.
      this.copyingItem_.message = 'Copying ...';
      this.copyingItem_.progressMax = event.status.totalBytes;
      this.copyingItem_.progressValue = event.status.processedBytes;
      progressCenter.addItem(this.copyingItem_);
      break;

    case 'PROGRESS':
      if (!this.copyingItem_) {
        console.error('Cannot find copying item.');
        return;
      }
      this.copyingItem_.progressValue = event.status.processedBytes;
      progressCenter.updateItem(this.copyingItem_);
      break;

    case 'SUCCESS':
      if (!this.copyingItem_) {
        console.error('Cannot find copying item.');
        return;
      }
      this.copyingItem_.progressValue = this.copyingItem_.progressMax;
      progressCenter.removeItem(this.copyingItem_.id);
      this.copyingItem_ = null;
      break;
  }
};

/**
 * Handles the delete event.
 * @param {Event} event The delete event.
 * @private
 */
ProgressCenterHandler.prototype.onDeleteProgress_ = function(event) {
  switch (event.reason) {
    case 'BEGIN':
      if (this.deletingItem_) {
        console.error('Previous delete is not completed.');
        return;
      }
      this.deletingItem_ = new ProgressCenterItem();
      // TODO(hirono): Specifying the correct message.
      this.deletingItem_.message = 'Deleting...';
      progressCenter.addItem(this.deletingItem_);
      break;

    case 'PROGRESS':
      if (!this.deletingItem_) {
        console.error('Cannot find deleting item.');
        return;
      }
      progressCenter.updateItem(this.deletingItem_);
      break;

    case 'SUCCESS':
      if (!this.deletingItem_) {
        console.error('Cannot find deleting item.');
        return;
      }
      progressCenter.removeItem(this.deletingItem_.id);
      this.deletingItem_ = null;
      break;
  }
};
