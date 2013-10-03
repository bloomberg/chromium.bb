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
  Object.freeze(this);

  // Register event handlers.
  element.addEventListener('click', this.onClick_.bind(this));
};

/**
 * Replaces the contents of this panel with the items.
 * @param {Array.<ProgressCenterItem>} items Items to be added to the panel.
 */
ProgressCenterPanel.prototype.setItems = function(items) {
  this.element_.innerText = '';
  for (var i = 0; i < items.length; i++) {
    this.addItem(items[i]);
  }
};

/**
 * Adds an item to the progress center panel.
 * @param {ProgressCenterItem} item Item to be added.
 */
ProgressCenterPanel.prototype.addItem = function(item) {
  this.openView_.insertBefore(
      this.createItemElement_(item),
      this.openView_.firstChild);
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
 * Removes an itme from the progress center panel.
 * @param {number} id progress item ID.
 */
ProgressCenterPanel.prototype.removeItem = function(id) {
  var itemElement = this.getItemElement_(id);
  if (!itemElement) {
    console.error('Invalid progress ID.');
    return;
  }
  this.openView_.removeChild(itemElement);
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
