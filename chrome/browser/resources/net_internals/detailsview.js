// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The DetailsView handles the tabbed view that displays either the "log" or
 * "timeline" view. This class keeps track of what the current view is, and
 * invalidates the specific view each time the selected data has changed.
 *
 * @constructor
 */
function DetailsView(logTabHandleId,
                     timelineTabHandleId,
                     detailsTabContentId) {
  // The DOM nodes that which contain the tab title.
  this.tabHandles_ = {};

  this.tabHandles_['timeline'] = document.getElementById(timelineTabHandleId);
  this.tabHandles_['log'] = document.getElementById(logTabHandleId);

  // The DOM node that contains the currently active tab sheet.
  this.contentArea_ = document.getElementById(detailsTabContentId);

  // Attach listeners to the "tab handles" so when you click them, it switches
  // active view.

  var self = this;

  this.tabHandles_['timeline'].onclick = function() {
    self.switchToView_('timeline');
  };

  this.tabHandles_['log'].onclick = function() {
    self.switchToView_('log');
  };

  this.currentData_ = [];

  // Default to the log view.
  this.switchToView_('log');
};

// The delay between updates to repaint.
DetailsView.REPAINT_TIMEOUT_MS = 50;

/**
 * Switches to the tab with name |viewName|. (Either 'log' or 'timeline'.
 */
DetailsView.prototype.switchToView_ = function(viewName) {
  if (this.currentView_) {
    // Remove the selected styling on currently selected tab.
    changeClassName(this.tabHandles_[this.currentView_], 'selected', false);
  }

  this.currentView_ = viewName;
  changeClassName(this.tabHandles_[this.currentView_], 'selected', true);
  this.repaint_();
};

/**
 * Updates the data this view is using.
 */
DetailsView.prototype.setData = function(currentData) {
  // Make a copy of the array (in case the caller mutates it), and sort it
  // by the source ID.
  this.currentData_ = DetailsView.createSortedCopy_(currentData);

  // Invalidate the view.
  if (!this.outstandingRepaint_) {
    this.outstandingRepaint_ = true;
    window.setTimeout(this.repaint_.bind(this),
                      DetailsView.REPAINT_TIMEOUT_MS);
  }
};

DetailsView.prototype.repaint_ = function() {
  this.outstandingRepaint_ = false;
  this.contentArea_.innerHTML = '';

  if (this.currentView_ == 'log') {
    PaintLogView(this.currentData_, this.contentArea_);
  } else {
    PaintTimelineView(this.currentData_, this.contentArea_);
  }
};

DetailsView.createSortedCopy_ = function(origArray) {
  var sortedArray = origArray.slice(0);
  sortedArray.sort(function(a, b) {
    return a.getSourceId() - b.getSourceId();
  });
  return sortedArray;
};
