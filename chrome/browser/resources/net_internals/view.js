// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Base class to represent a "view". A view is an absolutely positioned box on
 * the page.
 *
 * @constructor
 */
function View() {
  this.isVisible_ = true;
}

/**
 * Called to reposition the view on the page. Measurements are in pixels.
 */
View.prototype.setGeometry = function(left, top, width, height) {
  this.left_ = left;
  this.top_ = top;
  this.width_ = width;
  this.height_ = height;
};

/**
 * Called to show/hide the view.
 */
View.prototype.show = function(isVisible) {
  this.isVisible_ = isVisible;
};

View.prototype.isVisible = function() {
  return this.isVisible_;
};

/**
 * Method of the observer class.
 *
 * Called to check if an observer needs the data it is
 * observing to be actively updated.
 */
View.prototype.isActive = function() {
  return this.isVisible();
};

View.prototype.getLeft = function() {
  return this.left_;
};

View.prototype.getTop = function() {
  return this.top_;
};

View.prototype.getWidth = function() {
  return this.width_;
};

View.prototype.getHeight = function() {
  return this.height_;
};

View.prototype.getRight = function() {
  return this.getLeft() + this.getWidth();
};

View.prototype.getBottom = function() {
  return this.getTop() + this.getHeight();
};

View.prototype.setParameters = function(params) {};

//-----------------------------------------------------------------------------

/**
 * DivView is an implementation of View that wraps a DIV.
 *
 * @constructor
 */
function DivView(divId) {
  View.call(this);

  this.node_ = document.getElementById(divId);
  if (!this.node_)
    throw new Error('Element ' + divId + ' not found');

  // Initialize the default values to those of the DIV.
  this.width_ = this.node_.offsetWidth;
  this.height_ = this.node_.offsetHeight;
  this.isVisible_ = this.node_.style.display != 'none';
}

inherits(DivView, View);

DivView.prototype.setGeometry = function(left, top, width, height) {
  DivView.superClass_.setGeometry.call(this, left, top, width, height);

  this.node_.style.position = 'absolute';
  setNodePosition(this.node_, left, top, width, height);
};

DivView.prototype.show = function(isVisible) {
  DivView.superClass_.show.call(this, isVisible);
  setNodeDisplay(this.node_, isVisible);
};

/**
 * Returns the wrapped DIV
 */
DivView.prototype.getNode = function() {
  return this.node_;
};

//-----------------------------------------------------------------------------

/**
 * Implementation of View that sizes its child to fit the entire window.
 *
 * @param {!View} childView
 *
 * @constructor
 */
function WindowView(childView) {
  View.call(this);
  this.childView_ = childView;
  window.addEventListener('resize', this.resetGeometry.bind(this), true);
}

inherits(WindowView, View);

WindowView.prototype.setGeometry = function(left, top, width, height) {
  WindowView.superClass_.setGeometry.call(this, left, top, width, height);
  this.childView_.setGeometry(left, top, width, height);
};

WindowView.prototype.show = function() {
  WindowView.superClass_.show.call(this, isVisible);
  this.childView_.show(isVisible);
};

WindowView.prototype.resetGeometry = function() {
  this.setGeometry(0, 0, window.innerWidth, window.innerHeight);
};

