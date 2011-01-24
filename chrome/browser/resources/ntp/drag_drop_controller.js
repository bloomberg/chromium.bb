// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The delegate interface:
//   dragContainer -->
//         element containing the draggable items
//
//   transitionsDuration -->
//         length of time of transitions in ms
//
//   dragItem -->
//         get / set property containing the item being dragged
//
//   getItem(e) -->
//         get's the item that is under the mouse event |e|
//
//   canDropOn(coordinates) -->
//         returns true if the coordinates (relative to the drag container)
//         point to a valid place to drop an item
//
//   setDragPlaceholder(coordinates) -->
//         tells the delegate that the dragged item is currently above
//         the specified coordinates.
//
//   saveDrag() -->
//         tells the delegate that the drag is done. move the item to the
//         position last specified by setDragPlaceholder. (e.g., commit changes)
//

function DragAndDropController(delegate) {
  this.delegate_ = delegate;

  this.installHandlers_();
}

DragAndDropController.prototype = {
  startX_: 0,
  startY_: 0,
  startScreenX_: 0,
  startScreenY_: 0,

  installHandlers_: function() {
    var el = this.delegate_.dragContainer;
    el.addEventListener('dragstart', this.handleDragStart_.bind(this));
    el.addEventListener('dragenter', this.handleDragEnter_.bind(this));
    el.addEventListener('dragover', this.handleDragOver_.bind(this));
    el.addEventListener('dragleave', this.handleDragLeave_.bind(this));
    el.addEventListener('drop', this.handleDrop_.bind(this));
    el.addEventListener('dragend', this.handleDragEnd_.bind(this));
    el.addEventListener('drag', this.handleDrag_.bind(this));
    el.addEventListener('mousedown', this.handleMouseDown_.bind(this));
  },

  getCoordinates_: function(e) {
    var rect = this.delegate_.dragContainer.getBoundingClientRect();
    var coordinates = {
      x: e.clientX + window.scrollX - rect.left,
      y: e.clientY + window.scrollY - rect.top
    };

    // If we're in an RTL language, reflect the coordinates so the delegate
    // doesn't need to worry about it.
    if (isRtl())
      coordinates.x = this.delegate_.dragContainer.offsetWidth - coordinates.x;

    return coordinates;
  },

  // Listen to mousedown to get the relative position of the cursor when
  // starting drag and drop.
  handleMouseDown_: function(e) {
    var item = this.delegate_.getItem(e);
    if (!item)
      return;

    this.startX_ = item.offsetLeft;
    this.startY_ = item.offsetTop;
    this.startScreenX_ = e.screenX;
    this.startScreenY_ = e.screenY;

    // We don't want to focus the item on mousedown. However, to prevent
    // focus one has to call preventDefault but this also prevents the drag
    // and drop (sigh) so we only prevent it when the user is not doing a
    // left mouse button drag.
    if (e.button != 0) // LEFT
      e.preventDefault();
  },

  handleDragStart_: function(e) {
    var item = this.delegate_.getItem(e);
    if (!item)
      return;

    // Don't set data since HTML5 does not allow setting the name for
    // url-list. Instead, we just rely on the dragging of link behavior.
    this.delegate_.dragItem = item;
    item.classList.add('dragging');

    e.dataTransfer.effectAllowed = 'copyLinkMove';
  },

  handleDragEnter_: function(e) {
    if (this.delegate_.canDropOn(this.getCoordinates_(e)))
      e.preventDefault();
  },

  handleDragOver_: function(e) {
    var coordinates = this.getCoordinates_(e);
    if (!this.delegate_.canDropOn(coordinates))
      return;

    this.delegate_.setDragPlaceholder(coordinates);
    e.preventDefault();
    e.dataTransfer.dropEffect = 'move';
  },

  handleDragLeave_: function(e) {
    if (this.delegate_.canDropOn(this.getCoordinates_(e)))
      e.preventDefault();
  },

  handleDrop_: function(e) {
    var dragItem = this.delegate_.dragItem;
    if (!dragItem)
      return;

    this.delegate_.dragItem = null;
    this.delegate_.saveDrag();

    setTimeout(function() {
      dragItem.classList.remove('dragging');
    }, this.delegate_.transitionsDuration + 10);
  },

  handleDragEnd_: function(e) {
    return this.handleDrop_(e);
  },

  handleDrag_: function(e) {
    // Moves the drag item making sure that it is not displayed outside the
    // browser viewport.
    var dragItem = this.delegate_.dragItem;
    var rect = this.delegate_.dragContainer.getBoundingClientRect();

    var x = this.startX_ + e.screenX - this.startScreenX_;
    var y = this.startY_ + e.screenY - this.startScreenY_;

    // The position of the item is relative to #apps so we need to
    // subtract that when calculating the allowed position.
    x = Math.max(x, -rect.left);
    x = Math.min(x, document.body.clientWidth - rect.left -
                    dragItem.offsetWidth - 2);

    // The shadow is 2px
    y = Math.max(-rect.top, y);
    y = Math.min(y, document.body.clientHeight - rect.top -
                    dragItem.offsetHeight - 2);

    // Override right in case of RTL.
    dragItem.style.left = x + 'px';
    dragItem.style.top = y + 'px';
  }
};
