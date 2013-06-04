// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Drag selector used on the file list or the grid table.
 * TODO(hirono): Support drag selection for grid view. crbug.com/224832
 * @constructor
 */
function DragSelector() {
  /**
   * Target list of drag selection.
   * @type {cr.ui.List}
   * @private
   */
  this.target_ = null;

  /**
   * Border element of drag handle.
   * @type {HtmlElement}
   * @private
   */
  this.border_ = null;

  /**
   * Start point of dragging.
   * @type {number?}
   * @private
   */
  this.startX_ = null;

  /**
   * Start point of dragging.
   * @type {number?}
   * @private
   */
  this.startY_ = null;

  /**
   * Indexes of selected items by dragging at the last update.
   * @type {Array.<number>!}
   * @private
   */
  this.lastSelection_ = [];

  /**
   * Indexes of selected items at the start of dragging.
   * @type {Array.<number>!}
   * @private
   */
  this.originalSelection_ = [];

  // Bind handlers to make them removable.
  this.onMouseMoveBound_ = this.onMouseMove_.bind(this);
  this.onMouseUpBound_ = this.onMouseUp_.bind(this);
}

/**
 * Flag that shows whether the item is included in the selection or not.
 * @enum {number}
 * @private
 */
DragSelector.SelectionFlag_ = {
  IN_LAST_SELECTION: 1 << 0,
  IN_CURRENT_SELECTION: 1 << 1
};

/**
 * Starts drag selection by reacting dragstart event.
 * This function must be called from handlers of dragstart event.
 *
 * @this {DragSelector}
 * @param {cr.ui.List} list List where the drag selection starts.
 * @param {Event} event The dragstart event.
 */
DragSelector.prototype.startDragSelection = function(list, event) {
  // Precondition check
  if (!list.selectionModel_.multiple || this.target_)
    return;

  // Set the target of the drag selection
  this.target_ = list;

  // Create and add the border element
  if (!this.border_) {
    this.border_ = this.target_.ownerDocument.createElement('div');
    this.border_.className = 'drag-selection-border';
  }
  list.appendChild(this.border_);

  // Prevent default action.
  event.preventDefault();

  // If no modifier key is pressed, clear the original selection.
  if (!event.shiftKey && !event.ctrlKey) {
    this.target_.selectionModel_.unselectAll();
  }

  // Save the start state.
  var rect = list.getBoundingClientRect();
  this.startX_ = event.clientX - rect.left + list.scrollLeft;
  this.startY_ = event.clientY - rect.top + list.scrollTop;
  this.border_.style.left = this.startX_ + 'px';
  this.border_.style.top = this.startY_ + 'px';
  this.lastSelection_ = [];
  this.originalSelection_ = this.target_.selectionModel_.selectedIndexes;

  // Register event handlers.
  // The handlers are bounded at the constructor.
  this.target_.ownerDocument.addEventListener(
      'mousemove', this.onMouseMoveBound_, true);
  this.target_.ownerDocument.addEventListener(
      'mouseup', this.onMouseUpBound_, true);
};

/**
 * Handle the mousemove event.
 * @private
 * @param {MouseEvent} event The mousemove event.
 */
DragSelector.prototype.onMouseMove_ = function(event) {
  // Get the selection bounds.
  var inRect = this.target_.getBoundingClientRect();
  var x = event.clientX - inRect.left + this.target_.scrollLeft;
  var y = event.clientY - inRect.top + this.target_.scrollTop;
  var borderBounds = {
    left: Math.max(Math.min(this.startX_, x), 0),
    top: Math.max(Math.min(this.startY_, y), 0),
    right: Math.min(Math.max(this.startX_, x), this.target_.scrollWidth),
    bottom: Math.min(Math.max(this.startY_, y), this.target_.scrollHeight)
  };
  borderBounds.width = borderBounds.right - borderBounds.left;
  borderBounds.height = borderBounds.bottom - borderBounds.top;

  // Collect items within the selection rect.
  var currentSelection = [];
  var leadIndex = -1;
  for (var i = 0; i < this.target_.selectionModel_.length; i++) {
    // TODO(hirono): Stop to use the private method. crbug.com/246459
    var itemMetrics = this.target_.getHeightsForIndex_(i);
    itemMetrics.bottom = itemMetrics.top + itemMetrics.height;
    if (itemMetrics.top < borderBounds.bottom &&
        itemMetrics.bottom >= borderBounds.top) {
      currentSelection.push(i);
    }
    var pointed = itemMetrics.top <= y && y < itemMetrics.bottom;
    if (pointed)
      leadIndex = i;
  }

  // Diff the selection between currentSelection and this.lastSelection_.
  var selectionFlag = [];
  for (var i = 0; i < this.lastSelection_.length; i++) {
    var index = this.lastSelection_[i];
    // Bit operator can be used for undefined value.
    selectionFlag[index] =
        selectionFlag[index] | DragSelector.SelectionFlag_.IN_LAST_SELECTION;
  }
  for (var i = 0; i < currentSelection.length; i++) {
    var index = currentSelection[i];
    // Bit operator can be used for undefined value.
    selectionFlag[index] =
        selectionFlag[index] | DragSelector.SelectionFlag_.IN_CURRENT_SELECTION;
  }

  // Update the selection
  this.target_.selectionModel_.beginChange();
  for (var name in selectionFlag) {
    var index = parseInt(name);
    var flag = selectionFlag[name];
    // The flag may be one of followings:
    // - IN_LAST_SELECTION | IN_CURRENT_SELECTION
    // - IN_LAST_SELECTION
    // - IN_CURRENT_SELECTION
    // - undefined

    // If the flag equals to (IN_LAST_SELECTION | IN_CURRENT_SELECTION),
    // this is included in both the last selection and the current selection.
    // We have nothing to do for this item.

    if (flag == DragSelector.SelectionFlag_.IN_LAST_SELECTION) {
      // If the flag equals to IN_LAST_SELECTION,
      // then the item is included in lastSelection but not in currentSelection.
      // Revert the selection state to this.originalSelection_.
      this.target_.selectionModel_.setIndexSelected(
          index, this.originalSelection_.indexOf(index) != -1);
    } else if (flag == DragSelector.SelectionFlag_.IN_CURRENT_SELECTION) {
      // If the flag equals to IN_CURRENT_SELECTION,
      // this is included in currentSelection but not in lastSelection.
      this.target_.selectionModel_.setIndexSelected(index, true);
    }
  }
  if (leadIndex != -1) {
    this.target_.selectionModel_.leadIndex = leadIndex;
    this.target_.selectionModel_.anchorIndex = leadIndex;
  }
  this.target_.selectionModel_.endChange();
  this.lastSelection_ = currentSelection;

  // Update the size of border
  this.border_.style.left = borderBounds.left + 'px';
  this.border_.style.top = borderBounds.top + 'px';
  this.border_.style.width = borderBounds.width + 'px';
  this.border_.style.height = borderBounds.height + 'px';
};

/**
 * Handle the mouseup event.
 * @private
 * @param {MouseEvent} event The mouseup event.
 */
DragSelector.prototype.onMouseUp_ = function(event) {
  this.onMouseMove_(event);
  this.target_.removeChild(this.border_);
  this.target_.ownerDocument.removeEventListener(
      'mousemove', this.onMouseMoveBound_, true);
  this.target_.ownerDocument.removeEventListener(
      'mouseup', this.onMouseUpBound_, true);
  event.stopPropagation();
  this.target_ = null;
};
