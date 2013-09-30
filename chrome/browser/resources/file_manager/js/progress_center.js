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
 * @return {string} Progress item ID.
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
  return item.id;
};

/**
 * Updates the item in the progress center.
 *
 * TODO(hirono): Implement the method.
 *
 * @param {string} id ID of the item to be updated.
 * @param {ProgressCenterItem} item New contents of the item.
 */
ProgressCenter.prototype.updateItem = function(id, item) {};

/**
 * Removes the item in the progress center.
 *
 * TODO(hirono): Implement the method.
 *
 * @param {string} id ID of the item to be removed.
 */
ProgressCenter.prototype.removeItem = function(id) {};

/**
 * Switches the default container.
 * @param {ProgressItemContainer} newContainer New value of the default
 *     container.
 */
ProgressCenter.prototype.switchContainer = function(newContainer) {
  if (this.targetContainer_ == newContainer)
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
 * Passes the item to the ChromeOS's message center.
 *
 * TODO(hirono): Implement the method.
 *
 * @private
 */
ProgressCenter.prototype.passItemsToNotification_ = function() {

};
