// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The status view at the top of the page.  It displays what mode net-internals
 * is in (capturing, viewing only, viewing loaded log), and may have extra
 * information and actions depending on the mode.
 */
var StatusView = (function() {
  'use strict';

  // We inherit from View.
  var superClass = View;

  /**
   * Main entry point. Called once the page has loaded.
   * @constructor
   */
  function StatusView() {
    assertFirstConstructorCall(StatusView);

    superClass.call(this);

    this.subViews_ = {
      capture: new CaptureStatusView(),
      loaded: new LoadedStatusView(),
      halted: new HaltedStatusView()
    };

    this.activeSubViewName_ = 'capture';

    // Hide the non-active views.
    for (var k in this.subViews_) {
      if (k != this.activeSubViewName_)
        this.subViews_[k].show(false);
    }
  }

  cr.addSingletonGetter(StatusView);

  StatusView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    setGeometry: function(left, top, width, height) {
      superClass.prototype.setGeometry.call(this, left, top, width, height);
      this.getActiveSubView_().setGeometry(left, top, width, height);
    },

    getHeight: function() {
      return this.getActiveSubView_().getHeight();
    },

    show: function(isVisible) {
      superClass.prototype.show.call(this, isVisible);
      this.getActiveSubView_().show(isVisible);
    },

    setLayoutParent: function(view) {
      this.layoutParent_ = view;
    },

    /**
     * Switch the active subview.
     */
    switchToSubView: function(name) {
      if (!this.subViews_[name])
        throw 'Invalid subview name: ' + name;

      var prevSubView = this.getActiveSubView_();
      this.activeSubViewName_ = name;
      var newSubView = this.getActiveSubView_();

      prevSubView.show(false);
      newSubView.show(this.isVisible());

      // Since the subview's dimensions may have changed, re-trigger a layout
      // for our parent.
      var view = this.layoutParent_;
      view.setGeometry(view.getLeft(), view.getTop(),
                       view.getWidth(), view.getHeight());

      return newSubView;
    },

    getActiveSubView_: function() {
      return this.subViews_[this.activeSubViewName_];
    }
  };

  return StatusView;
})();
