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
   * Background page notifies item update to application windows.
   */
  ITEM_UPDATED: 'itemUpdated',

  /**
   * Background page notifies all the items are cleared.
   */
  RESET: 'reset'
});

/**
 * State of progress items.
 * @enum {string}
 * @const
 */
var ProgressItemState = Object.freeze({
  PROGRESSING: 'progressing',
  COMPLETE: 'complete',
  ERROR: 'error',
  CANCELED: 'canceled'
});

/**
 * Type of progress items.
 * @enum {string}
 * @const
 */
var ProgressItemType = Object.freeze({
  // The item is file copy operation.
  COPY: 'copy',
  // The item is file move operation.
  MOVE: 'move',
  // The item is file delete opeartion.
  DELETE: 'delete',
  // The item is file zip operation.
  ZIP: 'zip',
  // The item is general file transfer operation.
  // This is used for the mixed operation of summarized item.
  TRANSFER: 'transfer'
});

/**
 * Item of the progress center.
 * @constructor
 */
var ProgressCenterItem = function() {
  /**
   * Item ID.
   * @type {string}
   * @private
   */
  this.id_ = null;

  /**
   * State of the progress item.
   * @type {ProgressItemState}
   */
  this.state = ProgressItemState.PROGRESSING;

  /**
   * Message of the progress item.
   * @type {string}
   */
  this.message = '';

  /**
   * Max value of the progress.
   * @type {number}
   */
  this.progressMax = 0;

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

  /**
   * Type of progress item.
   */
  this.type = ProgressItemType.TRANSFER;

  /**
   * Whether the item is summarized item or not.
   * @type {boolean}
   */
  this.summarized = false;

  /**
   * Callback function to cancel the item.
   * @type {function()}
   */
  this.cancelCallback = null;

  Object.seal(this);
};

ProgressCenterItem.prototype = {
  /**
   * Setter of Item ID.
   * @param {string} value New value of ID.
   */
  set id(value) {
    if (!this.id_)
      this.id_ = value;
    else
      console.error('The ID is already set. (current ID: ' + this.id_ + ')');
  },

  /**
   * Getter of Item ID.
   * @return {string} Item ID.
   */
  get id() {
    return this.id_;
  },

  /**
   * Gets progress rate by percent.
   * @return {number} Progress rate by percent.
   */
  get progressRateByPercent() {
    return ~~(100 * this.progressValue / this.progressMax);
  },

  /**
   * Whether the item can be canceled or not.
   * @return {boolean} True if the item can be canceled.
   */
  get cancelable() {
    return !!(this.state == ProgressItemState.PROGRESSING &&
              this.cancelCallback &&
              !this.summarized);
  }
};
