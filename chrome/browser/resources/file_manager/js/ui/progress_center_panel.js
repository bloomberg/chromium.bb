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
  this.hidingTimeout_ = new ProgressCenterPanel.TimeoutManager(
      this.hideByTimeout_.bind(this));

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
 * Update item element.
 * @param {HTMLElement} element Element to be updated.
 * @param {ProgressCenterItem} item Progress center item.
 * @private
 */
ProgressCenterPanel.updateItemElement_ = function(element, item) {
  var additionalClass = item.state === ProgressItemState.COMPLETE ? 'complete' :
                        item.state === ProgressItemState.ERROR ? 'error' :
                        '';
  if (item.state === ProgressItemState.COMPLETE &&
      parseInt(element.querySelector('.progress-track').style.width) ===
          item.progressRateByPercent) {
    // Stop to update the message until the transition ends.
    element.setAttribute('data-complete-message', item.message);
    // The class pre-complete means that the actual operation is already done
    // but the UI transition of progress bar is not complete.
    additionalClass = 'pre-complete';
  } else {
    element.querySelector('label').textContent = item.message;
  }
  element.querySelector('.progress-track').style.width =
      item.progressRateByPercent + '%';
  element.setAttribute('data-progress-id', item.id);
  element.setAttribute('data-progress-max', item.progressMax);
  element.setAttribute('data-progress-value', item.progressValue);
  element.className = additionalClass;
};

/**
 * Updates an item to the progress center panel.
 * @param {ProgressCenterItem} item Item including new contents.
 */
ProgressCenterPanel.prototype.updateItem = function(item) {
  var itemElement = this.getItemElement_(item.id);
  if (!itemElement) {
    itemElement = this.createNewItemElement_();
    this.openView_.insertBefore(itemElement, this.openView_.firstNode);
  }
  ProgressCenterPanel.updateItemElement_(itemElement, item);
  if (item.state !== ProgressItemState.PROGRESSING)
    this.hidingTimeout_.request(ProgressCenterPanel.HIDE_DELAY_TIME_MS_);
  this.updateCloseView_();
  if (ProgressCenterPanel.ENABLED_)
    this.element_.hidden = false;
};

/**
 * Hides the progress center if there is no progressing items.
 * @private
 */
ProgressCenterPanel.prototype.hideByTimeout_ = function() {
  // If there are still progressing items, return the function.
  if (this.openView_.querySelectorAll('li:not(.complete):not(.error)').length)
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
  var itemElements = this.openView_.querySelectorAll('li:not(.error)');
  var activeElements = this.openView_.querySelectorAll(
      'li:not(.error):not(.complete)');
  var errorCount = this.openView_.querySelectorAll('li.error').length;
  var totalProgressMax = 0;
  var totalProgressValue = 0;
  for (var i = 0; i < itemElements.length; i++) {
    totalProgressMax += ~~itemElements[i].getAttribute('data-progress-max');
    totalProgressValue += ~~itemElements[i].getAttribute('data-progress-value');
  }
  var item = new ProgressCenterItem();
  item.progressMax = totalProgressMax;
  item.progressValue = totalProgressValue;
  item.state = errorCount ?
      ProgressItemState.ERROR : ProgressItemState.PROGRESSING;

  // TODO(hirono): Replace the messages with a string asset.
  if (activeElements.length == 0)
    if (errorCount > 1)
      item.message = errorCount + ' errors';
    else if (errorCount == 1)
      item.message = errorCount + ' error';
    else
      item.message = 'Complete';
  else if (activeElements.length == 1)
    item.message = activeElements[0].querySelector('label').textContent;
  else
    item.message = activeElements.length + ' active process';
  ProgressCenterPanel.updateItemElement_(this.closeViewItem_, item);
};

/**
 * Get an item element having the specified ID.
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

  itemElement.addEventListener(
      'webkitTransitionEnd', this.onItemTransitionEnd_.bind(this));

  return itemElement;
};

/**
 * Handles the transition end event of items.
 * @param {Event} event Transition end event.
 * @private
 */
ProgressCenterPanel.prototype.onItemTransitionEnd_ = function(event) {
  console.debug(event);
  if (event.currentTarget.className !== 'pre-complete' ||
      event.propertyName !== 'width')
    return;
  var completeMessage =
      event.currentTarget.getAttribute('data-complete-message');
  if (!completeMessage)
    return;
  event.currentTarget.className = 'complete';
  event.currentTarget.querySelector('label').textContent = completeMessage;
  this.updateCloseView_();
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
