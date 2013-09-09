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
   * @type {PreviewPanel.CalculatingSizeLabel}
   * @private
   */
  this.calculatingSizeLabel_ = new PreviewPanel.CalculatingSizeLabel(
      this.summaryElement_.querySelector('.calculating-size'));

  /**
   * @type {HTMLElement}
   * @private
   */
  this.previewText_ = element.querySelector('.preview-text');

  /**
   * FileSelection to be displayed.
   * @type {FileSelection}
   * @private
   */
  this.selection_ = {entries: [], computeBytes: function() {}};

  /**
   * Sequence value that is incremented by every selection update nad is used to
   * check if the callback is up to date or not.
   * @type {number}
   * @private
   */
  this.sequence_ = 0;

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
  // Preview panel shows when the selection property are set.
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
  this.updatePreviewText_();
  this.updateVisibility_();
};

/**
 * Apply the selection and update the view of the preview panel.
 * @param {FileSelection} selection Selection to be applied.
 */
PreviewPanel.prototype.setSelection = function(selection) {
  this.sequence_++;
  this.selection_ = selection;
  this.updateVisibility_();
  this.updatePreviewText_();
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
      newVisible = this.selection_.entries.length != 0 ||
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
 * Update the text in the preview panel.
 * @private
 */
PreviewPanel.prototype.updatePreviewText_ = function() {
  var selection = this.selection_;

  // Hides the preview text if zero or one file is selected. We shows a
  // breadcrumb list instead on the preview panel.
  if (selection.totalCount <= 1) {
    this.calculatingSizeLabel_.hidden = true;
    this.previewText_.textContent = '';
    return;
  }

  // Obtains the preview text.
  var text;
  if (selection.directoryCount == 0)
    text = strf('MANY_FILES_SELECTED', selection.fileCount);
  else if (selection.fileCount == 0)
    text = strf('MANY_DIRECTORIES_SELECTED', selection.directoryCount);
  else
    text = strf('MANY_ENTRIES_SELECTED', selection.totalCount);

  // Obtains the size of files.
  this.calculatingSizeLabel_.hidden = selection.bytesKnown;
  if (selection.bytesKnown && selection.showBytes)
    text += ', ' + util.bytesToString(selection.bytes);

  // Set the preview text to the element.
  this.previewText_.textContent = text;

  // Request the byte calculation if needed.
  if (!selection.bytesKnown) {
    this.selection_.computeBytes(function(sequence) {
      // Selection has been already updated.
      if (this.sequence_ != sequence)
        return;
      this.updatePreviewText_();
    }.bind(this, this.sequence_));
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

/**
 * Animating label that is shown during the bytes of selection entries is being
 * calculated.
 *
 * This label shows dots and varying the number of dots every
 * CalculatingSizeLabel.PERIOD milliseconds.
 * @param {HTMLElement} element DOM element of the label.
 * @constructor
 */
PreviewPanel.CalculatingSizeLabel = function(element) {
  this.element_ = element;
  this.count_ = 0;
  this.intervalID_ = null;
  Object.seal(this);
};

/**
 * Time period in milliseconds.
 * @const {number}
 */
PreviewPanel.CalculatingSizeLabel.PERIOD = 500;

PreviewPanel.CalculatingSizeLabel.prototype = {
  /**
   * Set visibility of the label.
   * When it is displayed, the text is animated.
   * @param {boolean} hidden Whether to hide the label or not.
   */
  set hidden(hidden) {
    this.element_.hidden = hidden;
    if (!hidden) {
      if (this.intervalID_ != null)
        return;
      this.count_ = 2;
      this.intervalID_ =
          setInterval(this.onStep_.bind(this),
                      PreviewPanel.CalculatingSizeLabel.PERIOD);
      this.onStep_();
    } else {
      if (this.intervalID_ == null)
        return;
      clearInterval(this.intervalID_);
      this.intervalID_ = null;
    }
  }
};

/**
 * Increments the counter and updates the number of dots.
 * @private
 */
PreviewPanel.CalculatingSizeLabel.prototype.onStep_ = function() {
  var text = str('CALCULATING_SIZE');
  for (var i = 0; i < ~~(this.count_ / 2) % 4; i++) {
    text += '.';
  }
  this.element_.textContent = text;
  this.count_++;
};
