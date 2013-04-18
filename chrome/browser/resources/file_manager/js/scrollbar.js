// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Creates a new scroll bar element.
 * @extends {HTMLDivElement}
 * @constructor
 */
var ScrollBar = cr.ui.define('div');

/**
 * Creates a vertical scrollbar.
 *
 * @param {Element} parent Parent element, must have a relative or absolute
 *     positioning.
 * @param {Element=} opt_scrollableArea Element with scrollable contents.
 *     If not passed, then call attachToView manually when the scrollable
 *     element becomes available.
 */
ScrollBar.createVertical = function(parent, opt_scrollableArea) {
  var scrollbar = new ScrollBar();
  parent.appendChild(scrollbar);
  if (opt_scrollableArea)
    scrollbar.attachToView(opt_scrollableArea);
};

/**
 * Mode of the scrollbar. As for now, only vertical scrollbars are supported.
 * @type {number}
 */
ScrollBar.Mode = {
  VERTICAL: 0,
  HORIZONTAL: 1
};

ScrollBar.prototype = {
  set mode(value) {
    this.mode_ = value;
    if (this.mode_ == ScrollBar.Mode.VERTICAL) {
      this.classList.remove('scrollbar-horizontal');
      this.classList.add('scrollbar-vertical');
    } else {
      this.classList.remove('scrollbar-vertical');
      this.classList.add('scrollbar-horizontal');
    }
    this.redraw_();
  },
  get mode() {
    return this.mode_;
  }
};

/**
 * Inherits after HTMLDivElement.
 */
ScrollBar.prototype.__proto__ = HTMLDivElement.prototype;

/**
 * Initializes the DOM structure of the scrollbar.
 */
ScrollBar.prototype.decorate = function() {
  this.classList.add('scrollbar');
  this.button_ = util.createChild(this, 'scrollbar-button', 'div');
  this.mode = ScrollBar.Mode.VERTICAL;

  this.button_.addEventListener('mousedown',
                                this.onButtonPressed_.bind(this));
  window.addEventListener('mouseup', this.onMouseUp_.bind(this));
  window.addEventListener('mousemove', this.onMouseMove_.bind(this));

  // Unfortunately we need to pool, since we can't easily detect resizing
  // and content changes.
  setInterval(this.redraw_.bind(this), 50);
};

/**
 * Attaches the scrollbar to a scrollable element and attaches handlers.
 * @param {Element} view Scrollable element.
 */
ScrollBar.prototype.attachToView = function(view) {
  this.view_ = view;
  this.view_.addEventListener('scroll', this.onScroll_.bind(this));
  this.redraw_();
};

/**
 * Scroll handler.
 * @private
 */
ScrollBar.prototype.onScroll_ = function() {
  this.redraw_();
};

/**
 * Pressing on the scrollbar's button handler.
 *
 * @param {Event} event Pressing event.
 * @private
 */
ScrollBar.prototype.onButtonPressed_ = function(event) {
  this.buttonPressed_ = true;
  this.buttonPressedEvent_ = event;
  this.buttonPressedPosition_ = this.button_.offsetTop - this.view_.offsetTop;
  this.button_.classList.add('pressed');

  event.preventDefault();
};

/**
 * Releasing the button handler. Note, that it may not be called when releasing
 * outside of the window. Therefore this is also called from onMouseMove_.
 *
 * @param {Event} event Mouse event.
 * @private
 */
ScrollBar.prototype.onMouseUp_ = function(event) {
  this.buttonPressed_ = false;
  this.button_.classList.remove('pressed');
};

/**
 * Mouse move handler. Updates the scroll position.
 *
 * @param {Event} event Mouse event.
 * @private
 */
ScrollBar.prototype.onMouseMove_ = function(event) {
  if (!this.buttonPressed_)
    return;
  if (!event.which) {
    this.onMouseUp_(event);
    return;
  }
  var clientSize = this.view_.clientHeight;
  var totalSize = this.view_.scrollHeight;
  var buttonSize = Math.max(50, clientSize / totalSize * clientSize);

  var buttonPosition = this.buttonPressedPosition_ +
      (event.screenY - this.buttonPressedEvent_.screenY);
  var scrollPosition = totalSize * (buttonPosition / clientSize);

  this.view_.scrollTop = scrollPosition;
  this.redraw_();
};

/**
 * Redraws the scrollbar.
 * @private
 */
ScrollBar.prototype.redraw_ = function() {
  if (!this.view_)
    return;

  var clientSize = this.view_.clientHeight;
  var clientTop = this.view_.offsetTop;
  var scrollPosition = this.view_.scrollTop;
  var totalSize = this.view_.scrollHeight;

  this.hidden = totalSize <= clientSize;

  var buttonSize = Math.max(50, clientSize / totalSize * clientSize);
  var buttonPosition = scrollPosition / (totalSize - clientSize) *
      (clientSize - buttonSize);

  this.button_.style.top = buttonPosition + clientTop + 'px';
  this.button_.style.height = buttonSize + 'px';
};
