// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Where to display the item.
 * @enum {string}
 * @const
 */
var ProgressItemContainer = Object.freeze({
  CLIENT: 'client',
  NOTIFICATION: 'notification'
});

/**
 * Messages to communicate between the background page and the application
 * windows.
 * @enum {string}
 * @const
 */
var ProgressCenterMessage = Object.freeze({
  /**
   * Application window obtains the current progress items.
   */
  GET_ITEMS: 'getItems',

  /**
   * Background page notifies item update to application windows.
   */
  ITEM_UPDATED: 'itemUpdated',

  /**
   * Background page notifies item added to application windows.
   */
  ITEM_ADDED: 'itemAdded',

  /**
   * Background page notifies item removed to application windows.
   */
  ITEM_REMOVED: 'itemRemoved'
});

/**
 * Item of the progress center.
 * @constructor
 */
var ProgressCenterItem = function() {
  /**
   * Message of the progress item.
   * @type {string}
   */
  this.message = '';

  /**
   * Max value of the progress.
   * @type {number}
   */
  this.progressMax = 1;

  /**
   * Current value of the progress.
   * @type {number}
   */
  this.progressValue = 0;

  /**
   * Where to the item is displayed.
   * @type {ProgressItemContainer}
   */
  this.container = ProgressItemContainer.CLIENT;

  Object.seal(this);
};
