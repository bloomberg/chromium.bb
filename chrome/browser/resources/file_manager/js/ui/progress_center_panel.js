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
  this.closeView_ = this.element_.querySelector('#progress-center-close-view');

  /**
   * Only progress item in the close view.
   * @type {!HTMLElement}
   * @private
   */
  this.closeViewItem_ = this.closeView_.querySelector('li');

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
  var element = this.createNewItemElement_();
  this.updateItemElement_(element, item);
  this.openView_.insertBefore(element, this.openView_.firstNode);
  this.updateCloseView_();
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
  this.updateItemElement_(itemElement, item);
  this.updateCloseView_();
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

  // Update view and set timeout function to remove.
  itemElement.classList.add('complete');
  itemElement.setAttribute('data-progress-value',
                           itemElement.getAttribute('data-progress-max'));
  this.updateCloseView_();
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
 * Updates close view summarizing the progress tasks.
 * @private
 */
ProgressCenterPanel.prototype.updateCloseView_ = function() {
  var itemElements = this.openView_.querySelectorAll('li');
  if (itemElements.length) {
    this.closeView_.hidden = false;
  } else {
    this.closeView_.hidden = true;
    return;
  }
  var totalProgressMax = 0;
  var totalProgressValue = 0;
  for (var i = 0; i < itemElements.length; i++) {
    totalProgressMax += ~~itemElements[i].getAttribute('data-progress-max');
    totalProgressValue += ~~itemElements[i].getAttribute('data-progress-value');
  }
  var item = new ProgressCenterItem();
  item.progressMax = totalProgressMax;
  item.progressValue = totalProgressValue;
  // TODO(hirono): Replace it with a string asset.
  item.message = itemElements.length == 1 ?
      itemElements[0].querySelector('label').textContent :
      itemElements.length + ' active process';
  this.updateItemElement_(this.closeViewItem_, item);
};

/**
 * Gets an item element having the specified ID.
 * @param {number} id progress item ID.
 * @return {HTMLElement} Item element having the ID.
 * @private
 */
ProgressCenterPanel.prototype.getItemElement_ = function(id) {
  var query = 'li[data-progress-id="$ID"]'.replace('$ID', id);
  return this.element_.querySelector(query);
};

/**
 * Creates an item element.
 * @return {HTMLElement} Created item element.
 * @private
 */
ProgressCenterPanel.prototype.createNewItemElement_ = function() {
  var label = this.element_.ownerDocument.createElement('label');
  label.className = 'label';

  var progressBarIndicator = this.element_.ownerDocument.createElement('div');
  progressBarIndicator.className = 'progress-track';

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
  itemElement.appendChild(label);
  itemElement.appendChild(progressFrame);

  return itemElement;
};

/**
 * Updates item element.
 * @param {HTMLElement} element Element to be updated.
 * @param {ProgressCenterItem} item Progress center item.
 * @private
 */
ProgressCenterPanel.prototype.updateItemElement_ = function(element, item) {
  element.querySelector('label').textContent = item.message;
  element.querySelector('.progress-track').style.width =
      item.progressRateByPercent + '%';
  element.setAttribute('data-progress-id', item.id);
  element.setAttribute('data-progress-max', item.progressMax);
  element.setAttribute('data-progress-value', item.progressValue);
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
