// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The class LayoutManager implements a vertically split layout that takes up
 * the whole window, and provides a draggable bar to resize the left and right
 * panes. Its elements are layed out as follows:
 *
 *
 *                  <<-- sizer -->>
 *
 *  +----------------------++----------------+
 *  |      topbar          ||                |
 *  +----------------------+|                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |      middlebox       ||    rightbox    |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  +----------------------++                |
 *  |      bottombar       ||                |
 *  +----------------------++----------------+
 *
 * The "topbar" and "bottombar" have a fixed height which is determined
 * by their initial content height. The rest of the boxes fill to occupy the
 * remaining space.
 *
 * The consumer provides DIVs for each of these regions, and the LayoutManager
 * positions them correctly in the window.
 *
 * @constructor
 */
function LayoutManager(topbarId,
                       middleboxId,
                       bottombarId,
                       sizerId,
                       rightboxId) {
  // Lookup the elements.
  this.topbar_ = document.getElementById(topbarId);
  this.middlebox_ = document.getElementById(middleboxId);
  this.bottombar_ = document.getElementById(bottombarId);
  this.sizer_ = document.getElementById(sizerId);
  this.rightbox_ = document.getElementById(rightboxId);

  // Make all the elements absolutely positioned.
  this.topbar_.style.position = "absolute";
  this.middlebox_.style.position = "absolute";
  this.bottombar_.style.position = "absolute";
  this.sizer_.style.position = "absolute";
  this.rightbox_.style.position = "absolute";

  // Set the initial split position at 50%.
  setNodeWidth(this.rightbox_, window.innerWidth / 2);

  // Setup the "sizer" so it can be dragged left/right to reposition the
  // vertical split.
  this.sizer_.addEventListener("mousedown", this.onDragSizerStart_.bind(this),
                               true);

  // Recalculate the layout whenever the window size changes.
  window.addEventListener("resize", this.recalculateLayout_.bind(this), true);

  // Do the initial layout .
  this.recalculateLayout_();
}

// Minimum width to size panels to, in pixels.
LayoutManager.MIN_PANEL_WIDTH = 50;

/**
 * Repositions all of the elements to fit the window.
 */
LayoutManager.prototype.recalculateLayout_ = function() {
  // Calculate the horizontal split points.
  var totalWidth = window.innerWidth;
  var rightboxWidth = this.rightbox_.offsetWidth;
  var sizerWidth = this.sizer_.offsetWidth;
  var leftPaneWidth = totalWidth - (rightboxWidth + sizerWidth);

  // Don't let the left pane get too small.
  if (leftPaneWidth < LayoutManager.MIN_PANEL_WIDTH) {
    leftPaneWidth = LayoutManager.MIN_PANEL_WIDTH;
    rightboxWidth = totalWidth - (sizerWidth + leftPaneWidth);
  }

  // Calculate the vertical split points.
  var totalHeight = window.innerHeight;
  var topbarHeight = this.topbar_.offsetHeight;
  var bottombarHeight = this.bottombar_.offsetHeight;
  var middleboxHeight = totalHeight - (topbarHeight + bottombarHeight);

  // Position the boxes using calculated split points.
  setNodePosition(this.topbar_, 0, 0,
                  leftPaneWidth, topbarHeight);
  setNodePosition(this.middlebox_, 0, topbarHeight,
                  leftPaneWidth,
                  middleboxHeight);
  setNodePosition(this.bottombar_, 0, (topbarHeight + middleboxHeight),
                  leftPaneWidth, bottombarHeight);

  setNodePosition(this.sizer_, leftPaneWidth, 0,
                  sizerWidth, totalHeight);
  setNodePosition(this.rightbox_, leftPaneWidth + sizerWidth, 0,
                  rightboxWidth, totalHeight);
};

/**
 * Called once we have clicked into the sizer. Starts capturing the mouse
 * position to implement dragging.
 */
LayoutManager.prototype.onDragSizerStart_ = function(event) {
  this.sizerMouseMoveListener_ = this.onDragSizer.bind(this);
  this.sizerMouseUpListener_ = this.onDragSizerEnd.bind(this);

  window.addEventListener("mousemove", this.sizerMouseMoveListener_, true);
  window.addEventListener("mouseup", this.sizerMouseUpListener_, true);

  event.preventDefault();
};

/**
 * Called when the mouse has moved after dragging started.
 */
LayoutManager.prototype.onDragSizer = function(event) {
  var newWidth = window.innerWidth - event.pageX;

  // Avoid shrinking the right box too much.
  newWidth = Math.max(newWidth, LayoutManager.MIN_PANEL_WIDTH);

  setNodeWidth(this.rightbox_, newWidth);
  this.recalculateLayout_();
};

/**
 * Called once the mouse has been released, and the dragging is over.
 */
LayoutManager.prototype.onDragSizerEnd = function(event) {
  window.removeEventListener("mousemove", this.sizerMouseMoveListener_, true);
  window.removeEventListener("mouseup", this.sizerMouseUpListener_, true);

  this.sizerMouseMoveListener_ = null;
  this.sizerMouseUpListener_ = null;

  event.preventDefault();
};
