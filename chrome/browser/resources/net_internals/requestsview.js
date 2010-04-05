// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * RequestsView displays a filtered list of all the requests, and a details
 * pane for the selected requests.
 *
 *  +----------------------++----------------+
 *  |      filter box      ||                |
 *  +----------------------+|                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |    requests list     ||    details     |
 *  |                      ||    view        |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  +----------------------++                |
 *  |      action bar      ||                |
 *  +----------------------++----------------+
 *
 * @constructor
 */
function RequestsView(tableBodyId, filterInputId, filterCountId,
                      deleteSelectedId, selectAllId,
                      tabHandlesContainerId, logTabId, timelineTabId,
                      detailsLogBoxId, detailsTimelineBoxId,
                      topbarId, middleboxId, bottombarId, sizerId) {
  View.call(this);

  // Initialize the sub-views.
  var leftPane = new TopMidBottomView(new DivView(topbarId),
                                      new DivView(middleboxId),
                                      new DivView(bottombarId));

  this.detailsView_ = new DetailsView(tabHandlesContainerId,
                                      logTabId,
                                      timelineTabId,
                                      detailsLogBoxId,
                                      detailsTimelineBoxId);

  this.splitterView_ = new ResizableVerticalSplitView(
      leftPane, this.detailsView_, new DivView(sizerId));

  this.sourceIdToEntryMap_ = {};
  this.currentSelectedSources_ = [];

  g_browser.addLogObserver(this);

  this.tableBody_ = document.getElementById(tableBodyId);

  this.filterInput_ = document.getElementById(filterInputId);
  this.filterCount_ = document.getElementById(filterCountId);

  this.filterInput_.addEventListener("search",
      this.onFilterTextChanged_.bind(this), true);

  document.getElementById(deleteSelectedId).onclick =
      this.deleteSelected_.bind(this);

  document.getElementById(selectAllId).addEventListener(
      'click', this.selectAll_.bind(this), true);

  this.currentFilter_ = '';
  this.numPrefilter_ = 0;
  this.numPostfilter_ = 0;

  this.invalidateFilterCounter_();
  this.invalidateDetailsView_();
}

inherits(RequestsView, View);

// How soon after updating the filter list the counter should be updated.
RequestsView.REPAINT_FILTER_COUNTER_TIMEOUT_MS = 0;

RequestsView.prototype.setGeometry = function(left, top, width, height) {
  RequestsView.superClass_.setGeometry.call(this, left, top, width, height);
  this.splitterView_.setGeometry(left, top, width, height);
};

RequestsView.prototype.show = function(isVisible) {
  RequestsView.superClass_.show.call(this, isVisible);
  this.splitterView_.show(isVisible);
};

RequestsView.prototype.onFilterTextChanged_ = function() {
  this.setFilter_(this.filterInput_.value);
};

RequestsView.prototype.setFilter_ = function(filterText) {
  this.currentFilter_ = filterText;

  // Iterate through all of the rows and see if they match the filter.
  for (var id in this.sourceIdToEntryMap_) {
    var entry = this.sourceIdToEntryMap_[id];
    entry.setIsMatchedByFilter(entry.matchesFilter(this.currentFilter_));
  }
};

RequestsView.prototype.onLogEntryAdded = function(logEntry) {
  // Lookup the source.
  var sourceEntry = this.sourceIdToEntryMap_[logEntry.source.id];

  if (!sourceEntry) {
    sourceEntry = new SourceEntry(this);
    this.sourceIdToEntryMap_[logEntry.source.id] = sourceEntry;
    this.incrementPrefilterCount(1);
  }

  sourceEntry.update(logEntry);

  if (sourceEntry.isSelected())
    this.invalidateDetailsView_();
};

RequestsView.prototype.incrementPrefilterCount = function(offset) {
  this.numPrefilter_ += offset;
  this.invalidateFilterCounter_();
};

RequestsView.prototype.incrementPostfilterCount = function(offset) {
  this.numPostfilter_ += offset;
  this.invalidateFilterCounter_();
};

RequestsView.prototype.onSelectionChanged = function() {
  this.invalidateDetailsView_();
};

RequestsView.prototype.clearSelection = function() {
  var prevSelection = this.currentSelectedSources_;
  this.currentSelectedSources_ = [];

  // Unselect everything that is currently selected.
  for (var i = 0; i < prevSelection.length; ++i) {
    prevSelection[i].setSelected(false);
  }

  this.onSelectionChanged();
};

RequestsView.prototype.deleteSelected_ = function() {
  var prevSelection = this.currentSelectedSources_;
  this.currentSelectedSources_ = [];

  for (var i = 0; i < prevSelection.length; ++i) {
    var entry = prevSelection[i];
    entry.remove();
    delete this.sourceIdToEntryMap_[entry.getSourceId()];
    this.incrementPrefilterCount(-1);
  }
};

RequestsView.prototype.selectAll_ = function(event) {
  for (var id in this.sourceIdToEntryMap_) {
    var entry = this.sourceIdToEntryMap_[id];
    if (entry.isMatchedByFilter()) {
      entry.setSelected(true);
    }
  }
  event.preventDefault();
};

RequestsView.prototype.modifySelectionArray = function(
    sourceEntry, addToSelection) {
  // Find the index for |sourceEntry| in the current selection list.
  var index = -1;
  for (var i = 0; i < this.currentSelectedSources_.length; ++i) {
    if (this.currentSelectedSources_[i] == sourceEntry) {
      index = i;
      break;
    }
  }

  if (index != -1 && !addToSelection) {
    // Remove from the selection.
    this.currentSelectedSources_.splice(index, 1);
  }

  if (index == -1 && addToSelection) {
    this.currentSelectedSources_.push(sourceEntry);
  }
}

RequestsView.prototype.invalidateDetailsView_ = function() {
  this.detailsView_.setData(this.currentSelectedSources_);
};

RequestsView.prototype.invalidateFilterCounter_ = function() {
  if (!this.outstandingRepaintFilterCounter_) {
    this.outstandingRepaintFilterCounter_ = true;
    window.setTimeout(this.repaintFilterCounter_.bind(this),
                      RequestsView.REPAINT_FILTER_COUNTER_TIMEOUT_MS);
  }
};

RequestsView.prototype.repaintFilterCounter_ = function() {
  this.outstandingRepaintFilterCounter_ = false;
  this.filterCount_.innerHTML = '';
  addTextNode(this.filterCount_,
              this.numPostfilter_ + " of " + this.numPrefilter_);
};
