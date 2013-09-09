// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * PreviewPanel UI class.
 * @param {HTMLElement} element DOM Element of preview panel.
 * @param {PreviewPanel.VisibilityType} visibilityType Initial value of the
 *     visibility type.
 * @param {string} currentPath Initial value of the current path.
 * @constructor
 * @extends {cr.EventTarget}
 */
var PreviewPanel = function(element, visibilityType, currentPath) {
  /**
   * The cached height of preview panel.
   * @type {number}
   * @private
   */
  this.height_ = 0;

  /**
   * Visiblity type of the preview panel.
   * @type {PreviewPanel.VisiblityType}
   * @private
   */
  this.visibilityType_ = visibilityType;

  /**
   * Current path to be desplayed.
   * @type {string}
   * @private
   */
  this.currentPath_ = currentPath || '/';

  /**
   * Dom element of the preview panel.
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.thumbnailElement_ = element.querySelector('.preview-thumbnails');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.summaryElement_ = element.querySelector('.preview-summary');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.textElement_ = element.querySelector('.preview-text');

  /**
   * Function to be called at the end of visibility change.
   * @type {function(boolean)}
   * @private
   */
  this.visibilityChangedCallback_ = null;

  /**
   * Entries to be displayed.
   * @type {Array.<Entry>}
   * @private
   */
  this.entries_ = [];

  cr.EventTarget.call(this);
};

/**
 * Name of PreviewPanels's event.
 * @enum {string}
 * @const
 */
PreviewPanel.Event = Object.freeze({
  // Event to be triggered at the end of visibility change.
  VISIBILITY_CHANGE: 'visibilityChange'
});

/**
 * Visibility type of the preview panel.
 */
PreviewPanel.VisibilityType = Object.freeze({
  // Preview panel always shows.
  ALWAYS_VISIBLE: 'alwaysVisible',
  // Preview panel shows when the entries property are set.
  AUTO: 'auto',
  // Preview panel does not show.
  ALWAYS_HIDDEN: 'alwaysHidden'
});

/**
 * @private
 */
PreviewPanel.Visibility_ = Object.freeze({
  VISIBLE: 'visible',
  HIDING: 'hiding',
  HIDDEN: 'hidden'
});

PreviewPanel.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * Setter for entries to be displayed in the preview panel.
   * @param {Array.<Entry>} entries New entries.
   */
  set entries(entries) {
    this.entries_ = entries;
    this.updateVisibility_();
  },

  /**
   * Setter for the current path.
   * @param {string} path New path.
   */
  set currentPath(path) {
    this.currentPath_ = path;
    this.updateVisibility_();
  },

  /**
   * Setter for the visibility type.
   * @param {PreviewPanel.VisibilityType} visibilityType New value of visibility
   *     type.
   */
  set visibilityType(visibilityType) {
    this.visibilityType_ = visibilityType;
    this.updateVisibility_();
  },

  get visible() {
    return this.element_.getAttribute('visibility') ==
        PreviewPanel.Visibility_.VISIBLE;
  },

  /**
   * Obtains the height of preview panel.
   * @return {number} Height of preview panel.
   */
  get height() {
    this.height_ = this.height_ || this.element_.clientHeight;
    return this.height_;
  }
};

/**
 * Initializes the element.
 */
PreviewPanel.prototype.initialize = function() {
  this.element_.addEventListener('webkitTransitionEnd',
                                 this.onTransitionEnd_.bind(this));
  this.updateVisibility_();
};

/**
 * Update the visibility of the preview panel.
 * @private
 */
PreviewPanel.prototype.updateVisibility_ = function() {
  // Get the new visibility value.
  var visibility = this.element_.getAttribute('visibility');
  var newVisible = null;
  switch (this.visibilityType_) {
    case PreviewPanel.VisibilityType.ALWAYS_VISIBLE:
      newVisible = true;
      break;
    case PreviewPanel.VisibilityType.AUTO:
      newVisible = this.entries_.length != 0 ||
                   !PathUtil.isRootPath(this.currentPath_);
      break;
    case PreviewPanel.VisibilityType.ALWAYS_HIDDEN:
      newVisible = false;
      break;
    default:
      console.error('Invalid visibilityType.');
      return;
  }

  // If the visibility has been already the new value, just return.
  if ((visibility == PreviewPanel.Visibility_.VISIBLE && newVisible) ||
      (visibility == PreviewPanel.Visibility_.HIDDEN && !newVisible))
    return;

  // Set the new visibility value.
  if (newVisible) {
    this.element_.setAttribute('visibility', PreviewPanel.Visibility_.VISIBLE);
    cr.dispatchSimpleEvent(this, PreviewPanel.Event.VISIBILITY_CHANGE);
  } else {
    this.element_.setAttribute('visibility', PreviewPanel.Visibility_.HIDING);
  }
};

/**
 * Event handler to be called at the end of hiding transition.
 * @param {Event} event The webkitTransitionEnd event.
 * @private
 */
PreviewPanel.prototype.onTransitionEnd_ = function(event) {
  if (event.target != this.element_ || event.propertyName != 'opacity')
    return;
  var visibility = this.element_.getAttribute('visibility');
  if (visibility != PreviewPanel.Visibility_.HIDING)
    return;
  this.element_.setAttribute('visibility', PreviewPanel.Visibility_.HIDDEN);
  cr.dispatchSimpleEvent(this, PreviewPanel.Event.VISIBILITY_CHANGE);
};
