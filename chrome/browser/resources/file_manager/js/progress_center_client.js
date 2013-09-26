// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * The message receiver of the background progress center.
 * @constructor
 * @param {function(Array.<ProgressItem>)} initializedCallback Callback to be
 *     called the client is initialized.
 * @param {function(ProgressItem)} itemAddedCallback Callback to be called an
 *     item is added to the progress center.
 */
var ProgressCenterClient = function(initializedCallback,
                                    itemAddedCallback) {
  this.initializedCallback_ = initializedCallback;
  this.itemAddedCallback_ = itemAddedCallback;

  // Send the initiali message.
  chrome.runtime.sendMessage({name: ProgressCenterMessage.GET_ITEMS},
                             this.onGetItems_.bind(this));
};

/**
 * Callback to be called when the initial items are obtained.
 * @param {Array.<ProgressItem>} items Initial items.
 * @private
 */
ProgressCenterClient.prototype.onGetItems_ = function(items) {
  // Register the handler.
  chrome.runtime.onMessage.addListener(this.onMessage_.bind(this));

  // Invoke the callback.
  this.initializedCallback_(items);
};

/**
 * Handle the messages from the background progress center.
 * @param {object} message Message to be sent from the background progress
 *     center.
 * @private
 */
ProgressCenterClient.prototype.onMessage_ = function(message) {
  switch (message.name) {
    case ProgressCenterMessage.ITEM_ADDED:
      this.itemAddedCallback_(message.item);
      break;
  }
};
