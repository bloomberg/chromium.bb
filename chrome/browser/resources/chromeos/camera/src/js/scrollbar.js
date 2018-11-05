// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Creates a scroll bar for a scrollable area.
 *
 * @param {cca.util.SmoothScroller} scoller Scroller for the scrollable
 *     element.
 * @constructor
 */
cca.ScrollBar = function(scroller) {
  /**
   * @type {cca.util.SmoothScroller}
   * @protected
   */
  this.scroller = scroller;

  /**
   * @type {?number}
   * @private
   */
  this.thumbLastScreenPosition_ = null;

  /**
   * @type {?number}
   * @private
   */
  this.domChangedTimer_ = null;

  /**
   * The scroll bar's track.
   * @type {HTMLElement}
   * @protected
   */
  this.track = document.createElement('div');

  /**
   * The scroll bar's thumb.
   * @type {HTMLElement}
   * @protected
   */
  this.thumb = document.createElement('div');

  /**
   * @type {boolean}
   * @private
   */
  this.thumbHidden_ = false;

  /**
   * @type {MutationObserver}
   * @private
   */
  this.domObserver_ = null;

  // End of properties. Seal the object.
  Object.seal(this);

  // Initialize the scroll bar's elements.
  this.track.classList.add('scrollbar-track');
  this.thumb.classList.add('scrollbar-thumb');

  this.scroller.element.appendChild(this.track);
  this.track.appendChild(this.thumb);

  // Add event handlers.
  this.thumb.addEventListener('mousedown',
                              this.onThumbPressed_.bind(this));
  window.addEventListener('mouseup', this.onMouseUp_.bind(this));
  window.addEventListener('mousemove', this.onMouseMove_.bind(this));

  this.scroller.element.addEventListener('scroll', this.onScroll_.bind(this));
  this.domObserver_ = new MutationObserver(this.onDomChanged_.bind(this));
  this.domObserver_.observe(
      this.scroller.element, {subtree: true, attributes: true});

  this.redraw_();
};

/**
 * Sets the scrollbar thumb hidden even for a scrollable area.
 * @param {boolean} hidden True if hides the thumb, false otherwise.
 */
cca.ScrollBar.prototype.setThumbHidden = function(hidden) {
  this.thumbHidden_ = hidden;
  this.redraw_();
};

/**
 * Scroll handler.
 * @private
 */
cca.ScrollBar.prototype.onScroll_ = function() {
  this.redraw_();
};

/**
 * Pressing on the scroll bar's button handler.
 * @param {Event} event Pressing event.
 * @private
 */
cca.ScrollBar.prototype.onThumbPressed_ = function(event) {
  this.thumbLastScreenPosition_ = this.getScreenPosition(event);
  this.thumb.classList.add('pressed');

  event.stopPropagation();
  event.preventDefault();
};

/**
 * Releasing the button handler. Note, that it may not be called when releasing
 * outside of the window. Therefore this is also called from onMouseMove_.
 *
 * @param {Event} event Mouse event.
 * @private
 */
cca.ScrollBar.prototype.onMouseUp_ = function(event) {
  this.thumbLastScreenPosition_ = null;
  this.thumb.classList.remove('pressed');
};

/**
 * @return {number} Total client size in pixels.
 * @protected
 */
cca.ScrollBar.prototype.getClientTotal = function() {
  throw new Error('Not implemented');
};

/**
 * @return {number} Total scroll size in pixels.
 * @protected
 */
cca.ScrollBar.prototype.getScrollTotal = function() {
  throw new Error('Not implemented');
};

/**
 * @param {Event} event Event.
 * @return {number} Total client position for the event in pixels.
 * @protected
 */
cca.ScrollBar.prototype.getClientPosition = function(event) {
  throw new Error('Not implemented');
};

/**
 * @param {Event} event Event.
 * @return {number} Total screen position for the event in pixels.
 * @protected
 */
cca.ScrollBar.prototype.getScreenPosition = function(event) {
  throw new Error('Not implemented');
};

/**
 * @return {number} Scroll position in pixels.
 * @protected
 */
cca.ScrollBar.prototype.getScrollPosition = function() {
  throw new Error('Not implemented');
};

/**
 * Sets the scroll position.
 * @param {number} value Position in pixels.
 * @protected
 */
cca.ScrollBar.prototype.setScrollPosition = function(value) {
  throw new Error('Not implemented');
};

/**
 * Sets geometry of the scroll bar's thumb.
 *
 * @param {number} position Position of the thumb in pixels.
 * @param {number} size Size of the thumb in pixels.
 * @protected
 */
cca.ScrollBar.prototype.setThumbGeometry = function(position, size) {
  throw new Error('Not implemented');
};

/**
 * Mouse move handler. Updates the scroll position.
 * @param {Event} event Mouse event.
 * @private
 */
cca.ScrollBar.prototype.onMouseMove_ = function(event) {
  if (!this.thumbLastScreenPosition_)
    return;

  if (!event.which) {
    this.onMouseUp_(event);
    return;
  }

  var clientTotal = this.getClientTotal();
  var scrollTotal = this.getScrollTotal();

  var scrollPosition = this.getScrollPosition() +
      (this.getScreenPosition(event) -
       this.thumbLastScreenPosition_) * (scrollTotal / clientTotal);

  this.setScrollPosition(scrollPosition);
  this.redraw_();

  this.thumbLastScreenPosition_ = this.getScreenPosition(event);
};

/**
 * Handles changed in Dom by redrawing the scroll bar. Ignores consecutive
 * calls.
 * @private
 */
cca.ScrollBar.prototype.onDomChanged_ = function() {
  if (this.domChangedTimer_) {
    clearTimeout(this.domChangedTimer_);
    this.domChangedTimer_ = null;
  }
  this.domChangedTimer_ = setTimeout(function() {
    this.redraw_();
    this.domChangedTimer_ = null;
  }.bind(this), 50);
};

/**
 * Resize handler to update the thumb size/position by redrawing the scroll bar.
 */
cca.ScrollBar.prototype.onResize = function() {
  this.redraw_();
};

/**
 * Redraws the scroll bar.
 * @private
 */
cca.ScrollBar.prototype.redraw_ = function() {
  var clientTotal = this.getClientTotal();
  var scrollTotal = this.getScrollTotal();

  this.thumb.hidden = this.thumbHidden_ || scrollTotal <= clientTotal;

  var thumbSize = Math.max(50, clientTotal / scrollTotal * clientTotal);
  var thumbPosition = this.getScrollPosition() / (scrollTotal - clientTotal) *
      (clientTotal - thumbSize);

  this.setThumbGeometry(thumbPosition, thumbSize);
};

/**
 * Creates a horizontal scroll bar.
 *
 * @param {cca.util.SmoothScroller} scoller Scroller for the scrollable
 *     element.
 * @constructor
 * @extends {cca.ScrollBar}
 */
cca.HorizontalScrollBar = function(scroller) {
  cca.ScrollBar.call(this, scroller);
  this.track.classList.add('scrollbar-track-horizontal');
};

cca.HorizontalScrollBar.prototype = {
  __proto__: cca.ScrollBar.prototype,
};

/**
 * @override
 */
cca.HorizontalScrollBar.prototype.getClientTotal = function() {
  return this.scroller.clientWidth;
};

/**
 * @override
 */
cca.HorizontalScrollBar.prototype.getScrollTotal = function() {
  return this.scroller.scrollWidth;
};

/**
 * @override
 */
cca.HorizontalScrollBar.prototype.getClientPosition = function(event) {
  return event.clientX;
};

/**
 * @override
 */
cca.HorizontalScrollBar.prototype.getScreenPosition = function(event) {
  return event.screenX;
};

/**
 * @override
 */
cca.HorizontalScrollBar.prototype.getScrollPosition = function() {
  return this.scroller.scrollLeft;
};

/**
 * @override
 */
cca.HorizontalScrollBar.prototype.setScrollPosition = function(value) {
  this.scroller.scrollTo(value,
                         this.scroller.element.scrollTop,
                         cca.util.SmoothScroller.Mode.INSTANT);
};

/**
 * @override
 */
cca.HorizontalScrollBar.prototype.setThumbGeometry = function(
    position, size) {
  this.thumb.style.left = position + 'px';
  this.thumb.style.width = size + 'px';
};
