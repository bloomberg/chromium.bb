// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Item element of the progress center.
 * @param {HTMLDocument} document Document which the new item belongs to.
 * @constructor
 */
function ProgressCenterItemElement(document) {
  var label = document.createElement('label');
  label.className = 'label';

  var progressBarIndicator = document.createElement('div');
  progressBarIndicator.className = 'progress-track';

  var progressBar = document.createElement('div');
  progressBar.className = 'progress-bar';
  progressBar.appendChild(progressBarIndicator);

  var cancelButton = document.createElement('button');
  cancelButton.className = 'cancel';
  cancelButton.setAttribute('tabindex', '-1');

  var progressFrame = document.createElement('div');
  progressFrame.className = 'progress-frame';
  progressFrame.appendChild(progressBar);
  progressFrame.appendChild(cancelButton);

  var itemElement = document.createElement('li');
  itemElement.appendChild(label);
  itemElement.appendChild(progressFrame);

  return ProgressCenterItemElement.decorate(itemElement);
}

/**
 * Event triggered when the item should be dismissed.
 * @type {string}
 * @const
 */
ProgressCenterItemElement.DISMISS_EVENT = 'dismiss';

/**
 * Decoreates the given element as a progress item.
 * @param {HTMLElement} element Item to be decoreated.
 * @return {ProgressCenterItemElement} Decoreated item.
 */
ProgressCenterItemElement.decorate = function(element) {
  element.__proto__ = ProgressCenterItemElement.prototype;
  element.state_ = ProgressItemState.PROGRESSING;
  element.timeoutId_ = null;
  element.track_ = element.querySelector('.progress-track');
  element.track_.addEventListener('webkitTransitionEnd',
                                  element.onTransitionEnd_.bind(element));
  element.nextWidthRate_ = null;
  element.timeoutId_ = null;
  return element;
};

ProgressCenterItemElement.prototype = {
  __proto__: HTMLDivElement.prototype,
  get animated() {
    return !!(this.timeoutId_ || this.track_.classList.contains('animated'));
  }
};

/**
 * Updates the element view according to the item.
 * @param {ProgressCenterItem} item Item to be referred for the update.
 */
ProgressCenterItemElement.prototype.update = function(item) {
  // If the current item is not progressing, the item no longer receives further
  // update.
  if (this.state_ === ProgressItemState.COMPLETED ||
      this.state_ === ProgressItemState.ERROR ||
      this.state_ === ProgressItemState.CANCELED)
    return;

  // Set element attributes.
  this.state_ = item.state;
  this.setAttribute('data-progress-id', item.id);
  this.classList.toggle('error', item.state === ProgressItemState.ERROR);
  this.classList.toggle('cancelable', item.cancelable);

  // Set label.
  if (this.state_ === ProgressItemState.PROGRESSING ||
      this.state_ === ProgressItemState.ERROR) {
    this.querySelector('label').textContent = item.message;
  } else if (this.state_ === ProgressItemState.CANCELED) {
    this.querySelector('label').textContent = '';
  }

  // Set track width.
  this.applyTrackWidth_(item.progressRateInPercent);

  // Check dismiss.
  this.checkDismiss_();
};

/**
 * Resets the item.
 */
ProgressCenterItemElement.prototype.reset = function() {
  this.track_.hidden = true;
  this.track_.width = '';
  this.state_ = ProgressItemState.PROGRESSING;
};

/**
 * Applies track width.
 * @param {number} nextWidthRate Next track width rate in percent.
 * @private
 */
ProgressCenterItemElement.prototype.applyTrackWidth_ = function(nextWidthRate) {
  this.nextWidthRate_ = nextWidthRate;
  this.timeoutId_ = this.timeoutId_ || setTimeout(function() {
    this.timeoutId_ = null;
    var currentWidthRate = parseInt(this.track_.style.width);
    if (currentWidthRate === this.nextWidthRate_ &&
        this.track_.classList.contains('animated'))
      return;
    var animated = !isNaN(currentWidthRate) && !isNaN(this.nextWidthRate_) &&
      currentWidthRate < this.nextWidthRate_;
    this.track_.hidden = false;
    this.track_.style.width = this.nextWidthRate_ + '%';
    this.track_.classList.toggle('animated', animated);
    this.checkDismiss_();
  }.bind(this));
};

/**
 * Handles transition end events.
 * @param {Event} event Transition end event.
 * @private
 */
ProgressCenterItemElement.prototype.onTransitionEnd_ = function(event) {
  if (event.propertyName !== 'width')
    return;
  this.track_.classList.remove('animated');
  this.checkDismiss_();
};

/**
 * Checks if the item should be dismissed or not.
 * If true, resets the item and publish a dismiss event.
 * @private
 */
ProgressCenterItemElement.prototype.checkDismiss_ = function() {
  if (this.animated ||
      this.state_ === ProgressItemState.PROGRESSING ||
      this.state_ === ProgressItemState.ERROR)
    return;

  this.reset();
  this.dispatchEvent(new Event(ProgressCenterItemElement.DISMISS_EVENT,
                               {bubbles: true}));
};

/**
 * Progress center panel.
 *
 * @param {HTMLElement} element DOM Element of the process center panel.
 * @constructor
 */
var ProgressCenterPanel = function(element) {
  /**
   * Root element of the progress center.
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * Open view containing multiple progress items.
   * @type {HTMLElement}
   * @private
   */
  this.openView_ = this.element_.querySelector('#progress-center-open-view');

  /**
   * Close view that is a summarized progress item.
   * @type {HTMLElement}
   * @private
   */
  this.closeView_ = ProgressCenterItemElement.decorate(
      this.element_.querySelector('#progress-center-close-view'));

  /**
   * Toggle animation rule of the progress center.
   * @type {CSSKeyFrameRule}
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
      ProgressCenterItemElement.DISMISS_EVENT,
      this.onItemDismiss_.bind(this));
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

  // Check whether the item should be displayed or not by referring its state.
  var itemElement = this.getItemElement_(item.id);
  switch (item.state) {
    // Should show the item.
    case ProgressItemState.PROGRESSING:
    case ProgressItemState.ERROR:
      // If the item has not been added yet, create a new element and add it.
      if (!itemElement) {
        itemElement =
            new ProgressCenterItemElement(this.element_.ownerDocument);
        this.openView_.insertBefore(itemElement, this.openView_.firstNode);
      }
      this.element_.hidden = false;
      break;

    // Should not show the item.
    case ProgressItemState.COMPLETED:
    case ProgressItemState.CANCELED:
      // If itemElement is not shown, just return.
      if (!itemElement)
        return;
      break;
  }

  // Update the element by referring the item model.
  itemElement.update(item);
};

/**
 * Updates close showing summarized item.
 * @param {!ProgressCenterItem} summarizedItem Item to be displayed in the close
 *     view.
 */
ProgressCenterPanel.prototype.updateCloseView = function(summarizedItem) {
  this.closeView_.classList.toggle('single', !summarizedItem.summarized);
  this.closeView_.update(summarizedItem);
};

/**
 * Remove all the items.
 * @param {boolean=} opt_force True if we force to reset and do not wait the
 *    transition of progress bar. False otherwise. False is default.
 */
ProgressCenterPanel.prototype.reset = function(opt_force) {
  // If an item is still animated, pending the reset.
  if (!opt_force) {
    var items = this.element_.querySelectorAll('li');
    var itemAnimated = false;
    for (var i = 0; i < items.length; i++) {
      itemAnimated = itemAnimated || items[i].animated;
    }

    if (itemAnimated) {
      this.resetRequested_ = true;
      return;
    }
  }

  // Clear the flag.
  this.resetRequested_ = false;

  // Clear the all compete item.
  this.openView_.innerHTML = '';

  // Clear track width of close view.
  this.closeView_.reset();

  // Hide the progress center.
  this.element_.hidden = true;
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
ProgressCenterPanel.prototype.onItemDismiss_ = function(event) {
  if (event.target !== this.closeView_)
    this.openView_.removeChild(event.target);

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
