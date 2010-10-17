// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view implements a vertically split display with a draggable divider.
 *
 *                  <<-- sizer -->>
 *
 *  +----------------------++----------------+
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |       leftView       ||   rightView    |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  +----------------------++----------------+
 *
 * @param {!View} leftView The widget to position on the left.
 * @param {!View} rightView The widget to position on the right.
 * @param {!DivView} sizerView The widget that will serve as draggable divider.
 * @constructor
 */
function ResizableVerticalSplitView(leftView, rightView, sizerView) {
  View.call(this);

  this.leftView_ = leftView;
  this.rightView_ = rightView;
  this.sizerView_ = sizerView;

  // Setup the "sizer" so it can be dragged left/right to reposition the
  // vertical split.
  sizerView.getNode().addEventListener(
      'mousedown', this.onDragSizerStart_.bind(this), true);
}

inherits(ResizableVerticalSplitView, View);

// Minimum width to size panels to, in pixels.
ResizableVerticalSplitView.MIN_PANEL_WIDTH = 50;

/**
 * Repositions all of the elements to fit the window.
 */
ResizableVerticalSplitView.prototype.setGeometry = function(
    left, top, width, height) {
  ResizableVerticalSplitView.superClass_.setGeometry.call(
      this, left, top, width, height);

  // If this is the first setGeometry(), initialize the split point at 50%.
  if (!this.leftSplit_)
    this.leftSplit_ = parseInt((width / 2).toFixed(0));

  // Calculate the horizontal split points.
  var leftboxWidth = this.leftSplit_;
  var sizerWidth = this.sizerView_.getWidth();
  var rightboxWidth = width - (leftboxWidth + sizerWidth);

  // Don't let the right pane get too small.
  if (rightboxWidth < ResizableVerticalSplitView.MIN_PANEL_WIDTH) {
    rightboxWidth = ResizableVerticalSplitView.MIN_PANEL_WIDTH;
    leftboxWidth = width - (sizerWidth + rightboxWidth);
  }

  // Position the boxes using calculated split points.
  this.leftView_.setGeometry(left, top, leftboxWidth, height);
  this.sizerView_.setGeometry(this.leftView_.getRight(), top,
                              sizerWidth, height);
  this.rightView_.setGeometry(this.sizerView_.getRight(), top,
                              rightboxWidth, height);
};

ResizableVerticalSplitView.prototype.show = function(isVisible) {
  ResizableVerticalSplitView.superClass_.show.call(this, isVisible);
  this.leftView_.show(isVisible);
  this.sizerView_.show(isVisible);
  this.rightView_.show(isVisible);
};

/**
 * Called once we have clicked into the sizer. Starts capturing the mouse
 * position to implement dragging.
 */
ResizableVerticalSplitView.prototype.onDragSizerStart_ = function(event) {
  this.sizerMouseMoveListener_ = this.onDragSizer.bind(this);
  this.sizerMouseUpListener_ = this.onDragSizerEnd.bind(this);

  window.addEventListener('mousemove', this.sizerMouseMoveListener_, true);
  window.addEventListener('mouseup', this.sizerMouseUpListener_, true);

  event.preventDefault();
};

/**
 * Called when the mouse has moved after dragging started.
 */
ResizableVerticalSplitView.prototype.onDragSizer = function(event) {
  // Convert from page coordinates, to view coordinates.
  this.leftSplit_ = (event.pageX - this.getLeft());

  // Avoid shrinking the left box too much.
  this.leftSplit_ = Math.max(
      this.leftSplit_, ResizableVerticalSplitView.MIN_PANEL_WIDTH);
  // Avoid shrinking the right box too much.
  this.leftSplit_ = Math.min(
      this.leftSplit_,
      this.getWidth() - ResizableVerticalSplitView.MIN_PANEL_WIDTH);

  // Force a layout with the new |leftSplit_|.
  this.setGeometry(
      this.getLeft(), this.getTop(), this.getWidth(), this.getHeight());
};

/**
 * Called once the mouse has been released, and the dragging is over.
 */
ResizableVerticalSplitView.prototype.onDragSizerEnd = function(event) {
  window.removeEventListener('mousemove', this.sizerMouseMoveListener_, true);
  window.removeEventListener('mouseup', this.sizerMouseUpListener_, true);

  this.sizerMouseMoveListener_ = null;
  this.sizerMouseUpListener_ = null;

  event.preventDefault();
};
