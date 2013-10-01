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
 * Event of the ProgressCenter class.
 * @enum {string}
 * @const
 */
var ProgressCenterEvent = Object.freeze({
  /**
   * Background page notifies item added to application windows.
   */
  ITEM_ADDED: 'itemAdded',

  /**
   * Background page notifies item update to application windows.
   */
  ITEM_UPDATED: 'itemUpdated',

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
   * Item ID.
   * @type {?number}
   * @private
   */
  this.id_ = null;

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

ProgressCenterItem.prototype = {
  /**
   * Setter of Item ID.
   * @param {number} value New value of ID.
   */
  set id(value) {
    if (!this.id_)
      this.id_ = value;
    else
      console.error('The ID is already set. (current ID: ' + this.id_ + ')');
  },

  /**
   * Getter of Item ID.
   * @return {number} Item ID.
   */
  get id() {
    return this.id_;
  }
};
