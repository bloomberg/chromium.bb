// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Progress center panel.
 *
 * @param {HTMLElement} element DOM Element of the process center panel.
 * @param {function(number)} cancelCallback Callback to becalled with the ID of
 *     the progress item when the cancel button is clicked.
 * @constructor
 */
var ProgressCenterPanel = function(element, cancelCallback) {
  this.element_ = element;
  this.openView_ = this.element_.querySelector('#progress-center-open-view');
  this.closeView_ = this.element_.querySelector('#progress-center-close-view');
  this.cancelCallback_ = cancelCallback;

  /**
   * Only progress item in the close view.
   * @type {!HTMLElement}
   * @private
   */
  this.closeViewItem_ = this.closeView_.querySelector('li');

  Object.freeze(this);

  // Register event handlers.
  element.addEventListener('click', this.onClick_.bind(this));
  element.addEventListener(
      'webkitTransitionEnd', this.onItemTransitionEnd_.bind(this));

};

/**
 * Whether to use the new progress center UI or not.
 * TODO(hirono): Remove the flag after replacing the old butter bar with the new
 * progress center.
 * @type {boolean}
 * @private
 */
ProgressCenterPanel.ENABLED_ = false;

/**
 * Update item element.
 * @param {HTMLElement} element Element to be updated.
 * @param {ProgressCenterItem} item Progress center item.
 * @private
 */
ProgressCenterPanel.updateItemElement_ = function(element, item) {
  var additionalClass = item.state === ProgressItemState.COMPLETE ? 'complete' :
                        item.state === ProgressItemState.ERROR ? 'error' :
                        item.state === ProgressItemState.CANCELED ? 'canceled' :
                        '';
  if (item.cancelable)
    additionalClass += ' cancelable';
  var previousWidthRate =
      parseInt(element.querySelector('.progress-track').style.width);
  if (item.state === ProgressItemState.COMPLETE &&
      previousWidthRate === item.progressRateByPercent) {
    // Stop to update the message until the transition ends.
    element.setAttribute('data-complete-message', item.message);
    // The class pre-complete means that the actual operation is already done
    // but the UI transition of progress bar is not complete.
    additionalClass = 'pre-complete';
  } else {
    element.querySelector('label').textContent = item.message;
    element.removeAttribute('data-complete-message');
  }
  // To commit the property change and to trigger the transition even if the
  // change is done synchronously, assign the width value asynchronously.
  setTimeout(function() {
    var track = element.querySelector('.progress-track');
    // When the progress rate is reverted, we does not use the transition
    // animation. Specifying '0' overrides the CSS settings and specifying null
    // re-enables it.
    track.style.transitionDuration =
        previousWidthRate > item.progressRateByPercent ? '0' : null;
    track.style.width = item.progressRateByPercent + '%';
  }, 0);
  // track.style.transitionDuration = null;
  element.setAttribute('data-progress-id', item.id);
  element.setAttribute('data-progress-max', item.progressMax);
  element.setAttribute('data-progress-value', item.progressValue);
  element.className = additionalClass;
};

/**
 * Updates an item to the progress center panel.
 * @param {ProgressCenterItem} item Item including new contents.
 * @param {ProgressCenterItem} summarizedItem Item to be desplayed in the close
 *     view.
 */
ProgressCenterPanel.prototype.updateItem = function(item, summarizedItem) {
  var itemElement = this.getItemElement_(item.id);
  if (!itemElement) {
    itemElement = this.createNewItemElement_();
    this.openView_.insertBefore(itemElement, this.openView_.firstNode);
  }
  ProgressCenterPanel.updateItemElement_(itemElement, item);

  // Update close view.
  this.closeView_.classList.toggle('single', !summarizedItem.summarized);
  ProgressCenterPanel.updateItemElement_(this.closeViewItem_, summarizedItem);

  if (ProgressCenterPanel.ENABLED_)
    this.element_.hidden = false;
};

/**
 * Remove all the items.
 */
ProgressCenterPanel.prototype.reset = function() {
  // Clear the all compete item.
  this.openView_.innerHTML = '';

  // Hide the progress center.
  this.element_.hidden = true;
  this.element_.classList.remove('opened');
};

/**
 * Gets an item element having the specified ID.
 * @param {number} id progress item ID.
 * @return {HTMLElement} Item element having the ID.
 * @private
 */
ProgressCenterPanel.prototype.getItemElement_ = function(id) {
  var query = 'li[data-progress-id="$ID"]'.replace('$ID', id);
  return this.openView_.querySelector(query);
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
 * Handles the transition end event of items.
 * @param {Event} event Transition end event.
 * @private
 */
ProgressCenterPanel.prototype.onItemTransitionEnd_ = function(event) {
  var itemElement = event.target.parentNode.parentNode.parentNode;
  if (itemElement.className !== 'pre-complete' ||
      event.propertyName !== 'width')
    return;
  var completeMessage = itemElement.getAttribute('data-complete-message');
  if (!completeMessage)
    return;
  itemElement.className = 'complete';
  itemElement.querySelector('label').textContent = completeMessage;
};

/**
 * Handles the click event.
 * @param {Event} event Click event.
 * @private
 */
ProgressCenterPanel.prototype.onClick_ = function(event) {
  if (event.target.classList.contains('toggle') &&
      !this.closeView_.classList.contains('single'))
    this.element_.classList.toggle('opened');
  else if ((event.target.classList.contains('toggle') &&
            this.closeView_.classList.contains('single')) ||
           event.target.classList.contains('cancel'))
    this.cancelCallback_(parseInt(
        event.target.parentNode.parentNode.getAttribute('data-progress-id')));
};
