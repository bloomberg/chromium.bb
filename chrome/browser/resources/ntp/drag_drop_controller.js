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
//   saveDrag(draggedItem) -->
//         tells the delegate that the drag is done. move the item to the
//         position last specified by setDragPlaceholder (e.g., commit changes).
//         draggedItem was the item being dragged.
//

// The distance, in px, that the mouse must move before initiating a drag.
var DRAG_THRESHOLD = 35;

function DragAndDropController(delegate) {
  this.delegate_ = delegate;

  // Install the 'mousedown' handler, the entry point to drag and drop.
  var el = this.delegate_.dragContainer;
  el.addEventListener('mousedown', this.handleMouseDown_.bind(this));
}

DragAndDropController.prototype = {
  isDragging_: false,
  startItem_: null,
  startItemXY_: null,
  startMouseXY_: null,
  mouseXY_: null,

  // Enables the handlers that are only active during a drag.
  enableHandlers_: function() {
    // Record references to the generated functions so we can
    // remove the listeners later.
    this.mouseMoveListener_ = this.handleMouseMove_.bind(this);
    this.mouseUpListener_ = this.handleMouseUp_.bind(this);
    this.scrollListener_ = this.handleScroll_.bind(this);

    document.addEventListener('mousemove', this.mouseMoveListener_, true);
    document.addEventListener('mouseup', this.mouseUpListener_, true);
    document.addEventListener('scroll', this.scrollListener_, true);
  },

  disableHandlers_: function() {
    document.removeEventListener('mousemove', this.mouseMoveListener_, true);
    document.removeEventListener('mouseup', this.mouseUpListener_, true);
    document.removeEventListener('scroll', this.scrollListener_, true);
  },

  isDragging: function() {
    return this.isDragging_;
  },

  distance_: function(p1, p2) {
    var x2 = Math.pow(p1.x - p2.x, 2);
    var y2 = Math.pow(p1.y - p2.y, 2);
    return Math.sqrt(x2 + y2);
  },

  // Shifts the client coordinates, |xy|, so they are relative to the top left
  // of the drag container.
  getCoordinates_: function(xy) {
    var rect = this.delegate_.dragContainer.getBoundingClientRect();
    var coordinates = {
      x: xy.x - rect.left,
      y: xy.y - rect.top
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

    // This can't be a drag & drop event if it's not the left mouse button
    // or if the mouse is not above an item. We also bail out if the dragging
    // flag is still set (the flag remains around for a bit so that 'click'
    // event handlers can distinguish between a click and drag).
    if (!item || e.button != 0 || this.isDragging())
      return;

    this.startItem_ = item;
    this.startItemXY_ = {x: item.offsetLeft, y: item.offsetTop};
    this.startMouseXY_ = {x: e.clientX, y: e.clientY};
    this.startScrollXY_ = {x: window.scrollX, y: window.scrollY};

    this.enableHandlers_();
  },

  handleMouseMove_: function(e) {
    this.mouseXY_ = {x: e.clientX, y: e.clientY};

    if (this.isDragging()) {
      this.handleDrag_();
      return;
    }

    // Initiate the drag if the mouse has moved far enough.
    if (this.distance_(this.startMouseXY_, this.mouseXY_) >= DRAG_THRESHOLD)
      this.handleDragStart_();
  },

  handleMouseUp_: function() {
    this.handleDrop_();
  },

  handleScroll_: function(e) {
    if (this.isDragging())
      this.handleDrag_();
  },

  handleDragStart_: function() {
    // Use the item that the mouse was above when 'mousedown' fired.
    var item = this.startItem_;
    if (!item)
      return;

    this.isDragging_ = true;
    this.delegate_.dragItem = item;
    item.classList.add('dragging');
    item.style.zIndex = 2;
  },

  handleDragOver_: function() {
    var coordinates = this.getCoordinates_(this.mouseXY_);
    if (!this.delegate_.canDropOn(coordinates))
      return;

    this.delegate_.setDragPlaceholder(coordinates);
  },

  handleDrop_: function() {
    this.disableHandlers_();

    var dragItem = this.delegate_.dragItem;
    if (!dragItem)
      return;

    this.delegate_.dragItem = this.startItem_ = null;
    this.delegate_.saveDrag(dragItem);
    dragItem.classList.remove('dragging');

    setTimeout(function() {
      // Keep the flag around a little so other 'mouseup' and 'click'
      // listeners know the event is from a drag operation.
      this.isDragging_ = false;
      dragItem.style.zIndex = 0;
    }.bind(this), this.delegate_.transitionsDuration);
  },

  handleDrag_: function() {
    // Moves the drag item making sure that it is not displayed outside the
    // drag container.
    var dragItem = this.delegate_.dragItem;
    var dragContainer = this.delegate_.dragContainer;
    var rect = dragContainer.getBoundingClientRect();

    // First, move the item the same distance the mouse has moved.
    var x = this.startItemXY_.x + this.mouseXY_.x - this.startMouseXY_.x +
              window.scrollX - this.startScrollXY_.x;
    var y = this.startItemXY_.y + this.mouseXY_.y - this.startMouseXY_.y +
              window.scrollY - this.startScrollXY_.y;

    var w = this.delegate_.dimensions.width;
    var h = this.delegate_.dimensions.height;

    var offset = parseInt(getComputedStyle(dragContainer).marginLeft);

    // The position of the item is relative to the drag container. We
    // want to make sure that half of the item's width or height is within
    // the container.
    x = Math.max(x, - w / 2 - offset);
    x = Math.min(x, rect.width  + w / 2 - offset);

    y = Math.max(- h / 2, y);
    y = Math.min(y, rect.height - h / 2);

    dragItem.style.left = x + 'px';
    dragItem.style.top = y + 'px';

    // Update the layouts and positions based on the new drag location.
    this.handleDragOver_();

    this.delegate_.scrollPage(this.mouseXY_);
  }
};
