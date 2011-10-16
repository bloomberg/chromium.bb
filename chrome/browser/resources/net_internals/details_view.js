// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DetailsView = (function() {
  'use strict';

  // We inherit from VerticalSplitView.
  var superClass = VerticalSplitView;

  /**
   * The DetailsView handles the tabbed view that displays either the "log" or
   * "timeline" view. This class keeps track of what the current view is, and
   * invalidates the specific view each time the selected data has changed.
   *
   * @constructor
   */
  function DetailsView(tabHandlesContainerId,
                       logTabId,
                       timelineTabId,
                       logBoxId,
                       timelineBoxId) {
    var tabSwitcher = new TabSwitcherView();

    superClass.call(this, new DivView(tabHandlesContainerId), tabSwitcher);

    this.tabSwitcher_ = tabSwitcher;

    this.logView_ = new DetailsLogView(logBoxId);
    this.timelineView_ = new DetailsTimelineView(timelineBoxId);

    this.tabSwitcher_.addTab(logTabId, this.logView_, true, true);
    this.tabSwitcher_.addTab(timelineTabId, this.timelineView_, true, true);

    // Default to the log view.
    this.tabSwitcher_.switchToTab(logTabId, null);
  }

  // The delay between updates to repaint.
  var REPAINT_TIMEOUT_MS = 50;

  DetailsView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Updates the data this view is using.
     */
    setData: function(currentData) {
      // Make a copy of the array (in case the caller mutates it), and sort it
      // by the source ID.
      var sortedCurrentData = createSortedCopy(currentData);

      // TODO(eroman): Should not access private members of TabSwitcherView.
      for (var i = 0; i < this.tabSwitcher_.tabs_.length; ++i)
        this.tabSwitcher_.tabs_[i].contentView.setData(sortedCurrentData);
    }
  };

  function createSortedCopy(origArray) {
    var sortedArray = origArray.slice(0);
    sortedArray.sort(function(a, b) {
      return a.getSourceId() - b.getSourceId();
    });
    return sortedArray;
  }

  //---------------------------------------------------------------------------

  var DetailsSubView = (function() {
    // We inherit from DivView.
    var superClass = DivView;

    /**
     * Base class for the Log view and Timeline view.
     *
     * @constructor
     */
    function DetailsSubView(boxId) {
      superClass.call(this, boxId);
      this.sourceEntries_ = [];
    }

    DetailsSubView.prototype = {
      // Inherit the superclass's methods.
      __proto__: superClass.prototype,

      setData: function(sourceEntries) {
        this.sourceEntries_ = sourceEntries;

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

    return DetailsSubView;
  })();

  //---------------------------------------------------------------------------

  var DetailsLogView = (function() {
    // We inherit from DetailsSubView.
    var superClass = DetailsSubView;

    /**
     * Subview that is displayed in the log tab.
     * @constructor
     */
    function DetailsLogView(boxId) {
      superClass.call(this, boxId);
    }

    DetailsLogView.prototype = {
      // Inherit the superclass's methods.
      __proto__: superClass.prototype,

      repaint: function() {
        superClass.prototype.repaint.call(this);
        PaintLogView(this.sourceEntries_, this.getNode());
      }
    };

    return DetailsLogView;
  })();

  //---------------------------------------------------------------------------

  var DetailsTimelineView = (function() {
    // We inherit from DetailsSubView.
    var superClass = DetailsSubView;

    /**
     * Subview that is displayed in the timeline tab.
     * @constructor
     */
    function DetailsTimelineView(boxId) {
      superClass.call(this, boxId);
    }

    DetailsTimelineView.prototype = {
      // Inherit the superclass's methods.
      __proto__: superClass.prototype,

      repaint: function() {
        superClass.prototype.repaint.call(this);
        PaintTimelineView(this.sourceEntries_, this.getNode());
      }
    };

    return DetailsTimelineView;
  })();

  return DetailsView;
})();
