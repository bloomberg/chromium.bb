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

  Object.seal(this);

  // Register the evnets.
  chrome.runtime.onMessage.addListener(this.onMessage_.bind(this));
};

ProgressCenter.prototype = {
  /**
   * Obtains the items to be displayed in the application window.
   * @private
   */
  get applicationItems_() {
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
  var message = {name: ProgressCenterMessage.ITEM_ADDED, item: item};
  this.sendMessage_(message);
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
    var items = this.applicationItems_;
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
 * Send a message to the target container.
 *
 * @param {object} message Message to be sent.
 * @private
 */
ProgressCenter.prototype.sendMessage_ = function(message) {
  switch (this.targetContainer_) {
    case ProgressItemContainer.CLIENT:
      chrome.runtime.sendMessage(message);
      break;
    case ProgressItemContainer.NOTIFICATION:
      // TODO(hirono): Implement it.
      break;
  }
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
 * Handles the messages form the client windows.
 * @param {*} message Message from the client windows.
 * @param {*} sender Sender of the message.
 * @param {function(*)} reply Function to reply values to the client windows.
 * @private
 */
ProgressCenter.prototype.onMessage_ = function(message, sender, reply) {
  switch (message.name) {
    case ProgressCenterMessage.GET_ITEMS:
      reply(this.applicationItems_);
      break;

    default:
      // Just ignore it.
      break;
  }
};
