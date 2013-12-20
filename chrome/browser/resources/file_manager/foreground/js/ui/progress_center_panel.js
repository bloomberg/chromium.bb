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
  /**
   * Root element of the progress center.
   * @type {!HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * Open view containing multiple progress items.
   * @type {!HTMLElement}
   * @private
   */
  this.openView_ = this.element_.querySelector('#progress-center-open-view');

  /**
   * Close view that is a summarized progress item.
   * @type {!HTMLElement}
   * @private
   */
  this.closeView_ = this.element_.querySelector('#progress-center-close-view');

  /**
   * Toggle animation rule of the progress center.
   * @type {!CSSKeyFrameRule}
   * @private
   */
  this.toggleAnimation_ = ProgressCenterPanel.getToggleAnimation_(
      element.ownerDocument);

  /**
   * Reset is requested but it is pending until the transition of progress bar
   * is complete.
   * @type {boolean}
   * @private
   */
  this.resetRequested_ = false;

  /**
   * Callback to becalled with the ID of the progress item when the cancel
   * button is clicked.
   */
  this.cancelCallback = null;

  Object.seal(this);

  // Register event handlers.
  element.addEventListener('click', this.onClick_.bind(this));
  element.addEventListener(
      'webkitAnimationEnd', this.onToggleAnimationEnd_.bind(this));
  element.addEventListener(
      'webkitTransitionEnd', this.onItemTransitionEnd_.bind(this));
};

/**
 * Updates attributes of the item element.
 * @param {!HTMLElement} element Element to be updated.
 * @param {!ProgressCenterItem} item Progress center item.
 * @private
 */
ProgressCenterPanel.updateItemElement_ = function(element, item) {
  // Sets element attributes.
  element.setAttribute('data-progress-id', item.id);
  element.classList.toggle('error', item.state === ProgressItemState.ERROR);
  element.classList.toggle('cancelable', item.cancelable);

  // Only when the previousWidthRate is not NaN (when style width is already
  // set) and the progress rate increases, we use transition animation.
  var previousWidthRate =
      parseInt(element.querySelector('.progress-track').style.width);
  var targetWidthRate = item.progressRateInPercent;
  var animation = !isNaN(previousWidthRate) &&
      previousWidthRate < targetWidthRate;
  if (item.state === ProgressItemState.COMPLETED && animation) {
    // The attribute pre-complete means that the actual operation is already
    // done but the UI transition of progress bar is not complete.
    element.setAttribute('pre-complete', '');
  } else {
    element.querySelector('label').textContent = item.message;
  }

  // To commit the property change and to trigger the transition even if the
  // change is done synchronously, assign the width value asynchronously.
  var updateTrackWidth = function() {
    var track = element.querySelector('.progress-track');
    track.classList.toggle('animated', animation);
    track.style.width = targetWidthRate + '%';
    track.hidden = false;
  };
  if (animation)
    setTimeout(updateTrackWidth);
  else
    updateTrackWidth();
};

/**
 * Obtains the toggle animation keyframes rule from the document.
 * @param {HTMLDocument} document Document containing the rule.
 * @return {CSSKeyFrameRules} Animation rule.
 * @private
 */
ProgressCenterPanel.getToggleAnimation_ = function(document) {
  for (var i = 0; i < document.styleSheets.length; i++) {
    var styleSheet = document.styleSheets[i];
    for (var j = 0; j < styleSheet.cssRules.length; j++) {
      var rule = styleSheet.cssRules[j];
      if (rule.type === CSSRule.WEBKIT_KEYFRAMES_RULE &&
          rule.name === 'progress-center-toggle') {
        return rule;
      }
    }
  }
  throw new Error('The progress-center-toggle rules is not found.');
};

/**
 * Updates an item to the progress center panel.
 * @param {!ProgressCenterItem} item Item including new contents.
 */
ProgressCenterPanel.prototype.updateItem = function(item) {
  // If reset is requested, force to reset.
  if (this.resetRequested_)
    this.reset(true);

  var itemElement = this.getItemElement_(item.id);

  // Check whether the item should be displayed or not by referring its state.
  switch (item.state) {
    // Should show the item.
    case ProgressItemState.PROGRESSING:
    case ProgressItemState.ERROR:
      // If the item has not been added yet, create a new element and add it.
      if (!itemElement) {
        itemElement = this.createNewItemElement_();
        this.openView_.insertBefore(itemElement, this.openView_.firstNode);
      }

      // Update the element by referring the item model.
      ProgressCenterPanel.updateItemElement_(itemElement, item);
      this.element_.hidden = false;
      break;

    // Should not show the item.
    case ProgressItemState.COMPLETED:
    case ProgressItemState.CANCELED:
      // If itemElement is not shown, just break.
      if (!itemElement)
        break;

      // If the item is complete state, once update it because it may turn to
      // have the pre-complete attribute.
      if (item.state === ProgressItemState.COMPLETED)
        ProgressCenterPanel.updateItemElement_(itemElement, item);

      // If the item has the pre-complete attribute, keep showing it. Otherwise,
      // just remove it.
      if (item.state !== ProgressItemState.COMPLETED ||
          !itemElement.hasAttribute('pre-complete')) {
        this.openView_.removeChild(itemElement);
      }
      break;
  }
};

/**
 * Updates close showing summarized item.
 * @param {!ProgressCenterItem} summarizedItem Item to be displayed in the close
 *     view.
 */
ProgressCenterPanel.prototype.updateCloseView = function(summarizedItem) {
  this.closeView_.classList.toggle('single', !summarizedItem.summarized);
  ProgressCenterPanel.updateItemElement_(this.closeView_, summarizedItem);
};

/**
 * Remove all the items.
 * @param {boolean=} opt_force True if we force to reset and do not wait the
 *    transition of progress bar. False otherwise. False is default.
 */
ProgressCenterPanel.prototype.reset = function(opt_force) {
  if (!opt_force && this.element_.querySelector('[pre-complete]')) {
    this.resetRequested_ = true;
    return;
  }

  // Clear the flag.
  this.resetRequested_ = false;

  // Clear the all compete item.
  this.openView_.innerHTML = '';

  // Clear track width of close view.
  this.closeView_.querySelector('.progress-track').style.width = '';

  // Hide the progress center.
  this.element_.hidden = true;
  this.closeView_.querySelector('.progress-track').hidden = true;
  this.element_.classList.remove('opened');
};

/**
 * Gets an item element having the specified ID.
 * @param {string} id progress item ID.
 * @return {HTMLElement} Item element having the ID.
 * @private
 */
ProgressCenterPanel.prototype.getItemElement_ = function(id) {
  var query = 'li[data-progress-id="' + id + '"]';
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
 * Handles the animation end event of the progress center.
 * @param {Event} event Animation end event.
 * @private
 */
ProgressCenterPanel.prototype.onToggleAnimationEnd_ = function(event) {
  // Transition end of the root element's height.
  if (event.target === this.element_ &&
      event.animationName === 'progress-center-toggle') {
    this.element_.classList.remove('animated');
    return;
  }
};

/**
 * Handles the transition end event of items.
 * @param {Event} event Transition end event.
 * @private
 */
ProgressCenterPanel.prototype.onItemTransitionEnd_ = function(event) {
  var itemElement = event.target.parentNode.parentNode.parentNode;
  if (!itemElement.hasAttribute('pre-complete') ||
      event.propertyName !== 'width')
    return;
  if (itemElement !== this.closeView_)
    this.openView_.removeChild(itemElement);
  itemElement.removeAttribute('pre-complete');

  if (this.resetRequested_)
    this.reset();
};

/**
 * Handles the click event.
 * @param {Event} event Click event.
 * @private
 */
ProgressCenterPanel.prototype.onClick_ = function(event) {
  // Toggle button.
  if (event.target.classList.contains('toggle') &&
      (!this.closeView_.classList.contains('single') ||
       this.element_.classList.contains('opened'))) {

    // If the progress center has already animated, just return.
    if (this.element_.classList.contains('animated'))
      return;

    // Obtains current and target height.
    var currentHeight;
    var targetHeight;
    if (this.element_.classList.contains('opened')) {
      currentHeight = this.openView_.getBoundingClientRect().height;
      targetHeight = this.closeView_.getBoundingClientRect().height;
    } else {
      currentHeight = this.closeView_.getBoundingClientRect().height;
      targetHeight = this.openView_.getBoundingClientRect().height;
    }

    // Set styles for animation.
    this.toggleAnimation_.cssRules[0].style.height = currentHeight + 'px';
    this.toggleAnimation_.cssRules[1].style.height = targetHeight + 'px';
    this.element_.classList.add('animated');
    this.element_.classList.toggle('opened');
    return;
  }

  // Cancel button.
  if (this.cancelCallback) {
    var id = event.target.classList.contains('toggle') ?
        this.closeView_.getAttribute('data-progress-id') :
        event.target.parentNode.parentNode.getAttribute('data-progress-id');
    this.cancelCallback(id);
  }
};
