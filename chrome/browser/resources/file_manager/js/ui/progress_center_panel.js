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
  Object.freeze(this);
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
  this.element_.appendChild(this.createItemElement_(item));
};

/**
 * Updates an item to the progress center panel.
 * @param {ProgressCenterItem} item Item including new contents.
 */
ProgressCenterPanel.prototype.updateItem = function(item) {
  var oldItemElement = this.getItemElement_(item.id);
  if (!oldItemElement) {
    console.error('Invalid progress ID.');
    return;
  }
  this.element_.replaceChild(this.createItemElement_(item), oldItemElement);
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
  this.element_.removeChild(itemElement);
};

/**
 * Get an item element having the specified ID.
 * @param {number} id progress item ID.
 * @return {HTMLElement} Item element having the ID.
 * @private
 */
ProgressCenterPanel.prototype.getItemElement_ = function(id) {
  var query =
      '.progress-center-item[data-progress-id="$ID"]'.replace('$ID', id);
  return this.element_.querySelector(query);
};

/**
 * Create an item element from ProgressCenterItem object.
 * @param {ProgressCenterItem} item Progress center item.
 * @return {HTMLElement} Created item element.
 * @private
 */
ProgressCenterPanel.prototype.createItemElement_ = function(item) {
  var itemLabel = this.element_.ownerDocument.createElement('label');
  itemLabel.className = 'progress-center-item-label';
  itemLabel.textContent = item.message;

  var itemProgressBar = this.element_.ownerDocument.createElement('div');
  itemProgressBar.className = 'progress-center-item-progress-bar';
  itemProgressBar.style.width =
      ~~(100 * item.progressValue / item.progressMax) + '%';

  var itemProgressFrame = this.element_.ownerDocument.createElement('div');
  itemProgressFrame.className = 'progress-center-item-progress-frame';
  itemProgressFrame.appendChild(itemProgressBar);

  var itemElement = this.element_.ownerDocument.createElement('div');
  itemElement.className = 'progress-center-item';
  itemElement.setAttribute('data-progress-id', item.id);
  itemElement.appendChild(itemLabel);
  itemElement.appendChild(itemProgressFrame);

  return itemElement;
};
