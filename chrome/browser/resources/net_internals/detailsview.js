// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  VerticalSplitView.call(this,
                         new DivView(tabHandlesContainerId),
                         tabSwitcher);

  this.tabSwitcher_ = tabSwitcher;

  this.logView_ = new DetailsLogView(logBoxId);
  this.timelineView_ = new DetailsTimelineView(timelineBoxId);

  this.tabSwitcher_.addTab(logTabId, this.logView_, true, true);
  this.tabSwitcher_.addTab(timelineTabId, this.timelineView_, true, true);

  // Default to the log view.
  this.tabSwitcher_.switchToTab(logTabId, null);
};

inherits(DetailsView, VerticalSplitView);

// The delay between updates to repaint.
DetailsView.REPAINT_TIMEOUT_MS = 50;

/**
 * Updates the data this view is using.
 */
DetailsView.prototype.setData = function(currentData) {
  // Make a copy of the array (in case the caller mutates it), and sort it
  // by the source ID.
  var sortedCurrentData = DetailsView.createSortedCopy_(currentData);

  // TODO(eroman): Should not access private members of TabSwitcherView.
  for (var i = 0; i < this.tabSwitcher_.tabs_.length; ++i)
    this.tabSwitcher_.tabs_[i].contentView.setData(sortedCurrentData);
};

DetailsView.createSortedCopy_ = function(origArray) {
  var sortedArray = origArray.slice(0);
  sortedArray.sort(function(a, b) {
    return a.getSourceId() - b.getSourceId();
  });
  return sortedArray;
};

//-----------------------------------------------------------------------------

/**
 * Base class for the Log view and Timeline view.
 *
 * @constructor
 */
function DetailsSubView(boxId) {
  DivView.call(this, boxId);
  this.sourceEntries_ = [];
}

inherits(DetailsSubView, DivView);

DetailsSubView.prototype.setData = function(sourceEntries) {
  this.sourceEntries_ = sourceEntries;

  // Repaint the view.
  if (this.isVisible() && !this.outstandingRepaint_) {
    this.outstandingRepaint_ = true;
    window.setTimeout(this.repaint.bind(this),
                      DetailsView.REPAINT_TIMEOUT_MS);
  }
};

DetailsSubView.prototype.repaint = function() {
  this.outstandingRepaint_ = false;
  this.getNode().innerHTML = '';
};

DetailsSubView.prototype.show = function(isVisible) {
  DetailsSubView.superClass_.show.call(this, isVisible);
  if (isVisible) {
    this.repaint();
  } else {
    this.getNode().innerHTML = '';
  }
};

//-----------------------------------------------------------------------------


/**
 * Subview that is displayed in the log tab.
 * @constructor
 */
function DetailsLogView(boxId) {
  DetailsSubView.call(this, boxId);
}

inherits(DetailsLogView, DetailsSubView);

DetailsLogView.prototype.repaint = function() {
  DetailsLogView.superClass_.repaint.call(this);
  PaintLogView(this.sourceEntries_, this.getNode());
};

//-----------------------------------------------------------------------------

/**
 * Subview that is displayed in the timeline tab.
 * @constructor
 */
function DetailsTimelineView(boxId) {
  DetailsSubView.call(this, boxId);
}

inherits(DetailsTimelineView, DetailsSubView);

DetailsTimelineView.prototype.repaint = function() {
  DetailsTimelineView.superClass_.repaint.call(this);
  PaintTimelineView(this.sourceEntries_, this.getNode());
};
