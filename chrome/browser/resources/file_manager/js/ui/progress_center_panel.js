// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Progress center panel.
 *
 * @param {HTMLElement} element DOM Element of the process center panel.
 * @constructor
 */
var ProgressCenterPanel = function(element) {
  this.element_ = element;
  this.openView_ = this.element_.querySelector('#progress-center-open-view');

  /**
   * Timeout callback to remove items.
   * @type {TimeoutManager}
   * @private
   */
  this.removingTimeout_ = new ProgressCenterPanel.TimeoutManager(
      this.completeItemRemoving_.bind(this));

  Object.freeze(this);

  // Register event handlers.
  element.addEventListener('click', this.onClick_.bind(this));
};

/**
 * The default amount of milliseconds time, before a progress item will hide
 * after the last update.
 * @type {number}
 * @private
 * @const
 */
ProgressCenterPanel.HIDE_DELAY_TIME_MS_ = 2000;

/**
 * Whether to use the new progress center UI or not.
 * TODO(hirono): Remove the flag after replacing the old butter bar with the new
 * progress center.
 * @type {boolean}
 * @private
 */
ProgressCenterPanel.ENABLED_ = false;

/**
 * Utility for timeout callback.
 *
 * @param {function(*):*} callback Callbakc function.
 * @constructor
 */
ProgressCenterPanel.TimeoutManager = function(callback) {
  this.callback_ = callback;
  this.id_ = null;
  Object.seal(this);
};

/**
 * Requests timeout. Previous request is canceled.
 * @param {number} milliseconds Time to invoke the callback function.
 */
ProgressCenterPanel.TimeoutManager.prototype.request = function(milliseconds) {
  if (this.id_)
    clearTimeout(this.id_);
  this.id_ = setTimeout(function() {
    this.id_ = null;
    this.callback_();
  }.bind(this), milliseconds);
};

/**
 * Adds an item to the progress center panel.
 * @param {ProgressCenterItem} item Item to be added.
 */
ProgressCenterPanel.prototype.addItem = function(item) {
  this.openView_.insertBefore(
      this.createItemElement_(item),
      this.openView_.firstChild);
  if (ProgressCenterPanel.ENABLED_)
    this.element_.hidden = false;
};

/**
 * Updates an item to the progress center panel.
 * @param {ProgressCenterItem} item Item including new contents.
 */
ProgressCenterPanel.prototype.updateItem = function(item) {
  var itemElement = this.getItemElement_(item.id);
  if (!itemElement) {
    console.error('Invalid progress ID.');
    return;
  }
  itemElement.querySelector('label').textContent = item.message;
  itemElement.querySelector('.progress-track').style.width =
      item.progressRateByPercent + '%';
};

/**
 * Removes an item from the progress center panel.
 * @param {number} id progress item ID.
 */
ProgressCenterPanel.prototype.removeItem = function(id) {
  // Obtain the item element.
  var itemElement = this.getItemElement_(id);
  if (!itemElement) {
    console.error('Invalid progress ID.');
    return;
  }

  // Check if already requested to remove.
  if (itemElement.classList.contains('complete'))
    return;

  // Set timeout function to remove.
  itemElement.classList.add('complete');
  this.removingTimeout_.request(ProgressCenterPanel.HIDE_DELAY_TIME_MS_);
};

/**
 * Completes item removing.
 * @param {HTMLElement} itemElement Element to be removed.
 * @private
 */
ProgressCenterPanel.prototype.completeItemRemoving_ = function(itemElement) {
  // If there are still progressing items, return the function.
  if (this.openView_.querySelectorAll('li:not(.complete)').length)
    return;

  // Clear the all compete item.
  this.openView_.innerHTML = '';

  // Hide the progress center.
  this.element_.hidden = true;
  this.element_.classList.remove('opened');
};

/**
 * Get an item element having the specified ID.
 * @param {number} id progress item ID.
 * @return {HTMLElement} Item element having the ID.
 * @private
 */
ProgressCenterPanel.prototype.getItemElement_ = function(id) {
  var query = '.item[data-progress-id="$ID"]'.replace('$ID', id);
  return this.element_.querySelector(query);
};

/**
 * Create an item element from ProgressCenterItem object.
 * @param {ProgressCenterItem} item Progress center item.
 * @return {HTMLElement} Created item element.
 * @private
 */
ProgressCenterPanel.prototype.createItemElement_ = function(item) {
  var label = this.element_.ownerDocument.createElement('label');
  label.className = 'label';
  label.textContent = item.message;

  var progressBarIndicator = this.element_.ownerDocument.createElement('div');
  progressBarIndicator.className = 'progress-track';
  progressBarIndicator.style.width = item.progressRateByPercent + '%';

  var progressBar = this.element_.ownerDocument.createElement('div');
  progressBar.className = 'progress-bar';
  progressBar.appendChild(progressBarIndicator);

  var cancelButton = this.element_.ownerDocument.createElement('button');
  cancelButton.className = 'cancel';
  cancelButton.setAttribute('tabindex', '-1');

  var progressFrame = this.element_.ownerDocument.createElement('div');
  progressFrame.className = 'progress-frame';
  progressFrame.appendChild(progressBar);
  progressFrame.appendChild(cancelButton);

  var itemElement = this.element_.ownerDocument.createElement('li');
  itemElement.className = 'item';
  itemElement.setAttribute('data-progress-id', item.id);
  itemElement.appendChild(label);
  itemElement.appendChild(progressFrame);

  return itemElement;
};

/**
 * Handles the click event.
 * @param {Event} event Click event.
 * @private
 */
ProgressCenterPanel.prototype.onClick_ = function(event) {
  if (event.target.classList.contains('toggle')) {
    this.element_.classList.toggle('opened');
  }
};
