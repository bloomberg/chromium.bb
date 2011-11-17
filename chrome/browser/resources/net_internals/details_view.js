// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DetailsView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * The DetailsView displays the "log" view. This class keeps track of the
   * selected SourceEntries, and repaints when they change.
   *
   * @constructor
   */
  function DetailsView(boxId) {
    superClass.call(this, boxId);
    this.sourceEntries_ = [];
  }

  // The delay between updates to repaint.
  var REPAINT_TIMEOUT_MS = 50;

  DetailsView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    setData: function(sourceEntries) {
      // Make a copy of the array (in case the caller mutates it), and sort it
      // by the source ID.
      this.sourceEntries_ = createSortedCopy(sourceEntries);

      // Repaint the view.
      if (this.isVisible() && !this.outstandingRepaint_) {
        this.outstandingRepaint_ = true;
        window.setTimeout(this.repaint.bind(this),
                          REPAINT_TIMEOUT_MS);
      }
    },

    repaint: function() {
      this.outstandingRepaint_ = false;
      this.getNode().innerHTML = '';
      PaintLogView(this.sourceEntries_, this.getNode());
    },

    show: function(isVisible) {
      superClass.prototype.show.call(this, isVisible);
      if (isVisible) {
        this.repaint();
      } else {
        this.getNode().innerHTML = '';
      }
    }
  };

  function createSortedCopy(origArray) {
    var sortedArray = origArray.slice(0);
    sortedArray.sort(function(a, b) {
      return a.getSourceId() - b.getSourceId();
    });
    return sortedArray;
  }

  return DetailsView;
})();
