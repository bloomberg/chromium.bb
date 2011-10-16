// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TopMidBottomView = (function() {
  'use strict';

  // We inherit from View.
  var superClass = View;

  /**
   * This view stacks three boxes -- one at the top, one at the bottom, and
   * one that fills the remaining space between those two.
   *
   *  +----------------------+
   *  |      topbar          |
   *  +----------------------+
   *  |                      |
   *  |                      |
   *  |                      |
   *  |                      |
   *  |      middlebox       |
   *  |                      |
   *  |                      |
   *  |                      |
   *  |                      |
   *  |                      |
   *  +----------------------+
   *  |     bottombar        |
   *  +----------------------+
   *
   *  @constructor
   */
  function TopMidBottomView(topView, midView, bottomView) {
    superClass.call(this);

    this.topView_ = topView;
    this.midView_ = midView;
    this.bottomView_ = bottomView;
  }

  TopMidBottomView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    setGeometry: function(left, top, width, height) {
      superClass.prototype.setGeometry.call(this, left, top, width, height);

      // Calculate the vertical split points.
      var topbarHeight = this.topView_.getHeight();
      var bottombarHeight = this.bottomView_.getHeight();
      var middleboxHeight = height - (topbarHeight + bottombarHeight);

      // Position the boxes using calculated split points.
      this.topView_.setGeometry(left, top, width, topbarHeight);
      this.midView_.setGeometry(left, this.topView_.getBottom(),
                                width, middleboxHeight);
      this.bottomView_.setGeometry(left, this.midView_.getBottom(),
                                   width, bottombarHeight);
    },

    show: function(isVisible) {
      superClass.prototype.show.call(this, isVisible);
      this.topView_.show(isVisible);
      this.midView_.show(isVisible);
      this.bottomView_.show(isVisible);
    }
  };

  return TopMidBottomView;
})();
