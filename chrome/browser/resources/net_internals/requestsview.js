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
                      deleteSelectedId, deleteAllId, selectAllId, sortByIdId,
                      sortBySourceTypeId, sortByDescriptionId,
                      tabHandlesContainerId, logTabId, timelineTabId,
                      detailsLogBoxId, detailsTimelineBoxId,
                      topbarId, middleboxId, bottombarId, sizerId) {
  View.call(this);

  // Used for sorting entries with automatically assigned IDs.
  this.maxReceivedSourceId_ = 0;

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

  g_browser.addLogObserver(this);

  this.tableBody_ = document.getElementById(tableBodyId);

  this.filterInput_ = document.getElementById(filterInputId);
  this.filterCount_ = document.getElementById(filterCountId);

  this.filterInput_.addEventListener("search",
      this.onFilterTextChanged_.bind(this), true);

  document.getElementById(deleteSelectedId).onclick =
      this.deleteSelected_.bind(this);

  document.getElementById(deleteAllId).onclick =
      g_browser.deleteAllEvents.bind(g_browser);

  document.getElementById(selectAllId).addEventListener(
      'click', this.selectAll_.bind(this), true);

  document.getElementById(sortByIdId).addEventListener(
      'click', this.sortById_.bind(this), true);

  document.getElementById(sortBySourceTypeId).addEventListener(
      'click', this.sortBySourceType_.bind(this), true);

  document.getElementById(sortByDescriptionId).addEventListener(
      'click', this.sortByDescription_.bind(this), true);

  // Sets sort order and filter.
  this.setFilter_('');

  this.initializeSourceList_();
}

inherits(RequestsView, View);

/**
 * Initializes the list of source entries.  If source entries are already,
 * being displayed, removes them all in the process.
 */
RequestsView.prototype.initializeSourceList_ = function() {
  this.currentSelectedSources_ = [];
  this.sourceIdToEntryMap_ = {};
  this.tableBody_.innerHTML = '';
  this.numPrefilter_ = 0;
  this.numPostfilter_ = 0;
  this.invalidateFilterCounter_();
  this.invalidateDetailsView_();
};

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

RequestsView.prototype.getFilterText_ = function() {
  return this.filterInput_.value;
};

RequestsView.prototype.setFilterText_ = function(filterText) {
  this.filterInput_.value = filterText;
  this.onFilterTextChanged_();
};

RequestsView.prototype.onFilterTextChanged_ = function() {
  this.setFilter_(this.getFilterText_());
};

/**
 * Sorts active entries first.   If both entries are inactive, puts the one
 * that was active most recently first.  If both are active, uses source ID,
 * which puts longer lived events at the top, and behaves better than using
 * duration or time of first event.
 */
RequestsView.compareActive_ = function(source1, source2) {
  if (source1.isActive() && !source2.isActive())
    return -1;
  if (!source1.isActive() && source2.isActive())
    return  1;
  if (!source1.isActive()) {
    var deltaEndTime = source1.getEndTime() - source2.getEndTime();
    if (deltaEndTime != 0) {
      // The one that ended most recently (Highest end time) should be sorted
      // first.
      return -deltaEndTime;
    }
    // If both ended at the same time, then odds are they were related events,
    // started one after another, so sort in the opposite order of their
    // source IDs to get a more intuitive ordering.
    return -RequestsView.compareSourceId_(source1, source2);
  }
  return RequestsView.compareSourceId_(source1, source2);
};

RequestsView.compareDescription_ = function(source1, source2) {
  var source1Text = source1.getDescription().toLowerCase();
  var source2Text = source2.getDescription().toLowerCase();
  var compareResult = source1Text.localeCompare(source2Text);
  if (compareResult != 0)
    return compareResult;
  return RequestsView.compareSourceId_(source1, source2);
};

RequestsView.compareDuration_ = function(source1, source2) {
  var durationDifference = source2.getDuration() - source1.getDuration();
  if (durationDifference)
    return durationDifference;
  return RequestsView.compareSourceId_(source1, source2);
};

/**
 * For the purposes of sorting by source IDs, entries without a source
 * appear right after the SourceEntry with the highest source ID received
 * before the sourceless entry. Any ambiguities are resolved by ordering
 * the entries without a source by the order in which they were received.
 */
RequestsView.compareSourceId_ = function(source1, source2) {
  var sourceId1 = source1.getSourceId();
  if (sourceId1 < 0)
    sourceId1 = source1.getMaxPreviousEntrySourceId();
  var sourceId2 = source2.getSourceId();
  if (sourceId2 < 0)
    sourceId2 = source2.getMaxPreviousEntrySourceId();

  if (sourceId1 != sourceId2)
    return sourceId1 - sourceId2;

  // One or both have a negative ID. In either case, the source with the
  // highest ID should be sorted first.
  return source2.getSourceId() - source1.getSourceId();
};

RequestsView.compareSourceType_ = function(source1, source2) {
  var source1Text = source1.getSourceTypeString();
  var source2Text = source2.getSourceTypeString();
  var compareResult = source1Text.localeCompare(source2Text);
  if (compareResult != 0)
    return compareResult;
  return RequestsView.compareSourceId_(source1, source2);
};

RequestsView.prototype.comparisonFuncWithReversing_ = function(a, b) {
  var result = this.comparisonFunction_(a, b);
  if (this.doSortBackwards_)
    result *= -1;
  return result;
};

RequestsView.comparisonFunctionTable_ = {
  // sort: and sort:- are allowed
  '':            RequestsView.compareSourceId_,
  'active':      RequestsView.compareActive_,
  'desc':        RequestsView.compareDescription_,
  'description': RequestsView.compareDescription_,
  'duration':    RequestsView.compareDuration_,
  'id':          RequestsView.compareSourceId_,
  'source':      RequestsView.compareSourceType_,
  'type':        RequestsView.compareSourceType_
};

RequestsView.prototype.Sort_ = function() {
  var sourceEntries = [];
  for (var id in this.sourceIdToEntryMap_) {
    // Can only sort items with an actual row in the table.
    if (this.sourceIdToEntryMap_[id].hasRow())
      sourceEntries.push(this.sourceIdToEntryMap_[id]);
  }
  sourceEntries.sort(this.comparisonFuncWithReversing_.bind(this));

  for (var i = sourceEntries.length - 2; i >= 0; --i) {
    if (sourceEntries[i].getNextNodeSourceId() !=
        sourceEntries[i + 1].getSourceId())
      sourceEntries[i].moveBefore(sourceEntries[i + 1]);
  }
};

/**
 * Looks for the first occurence of |directive|:parameter in |sourceText|.
 * Parameter can be an empty string.
 *
 * On success, returns an object with two fields:
 *   |remainingText| - |sourceText| with |directive|:parameter removed,
                       and excess whitespace deleted.
 *   |parameter| - the parameter itself.
 *
 * On failure, returns null.
 */
RequestsView.prototype.parseDirective_ = function(sourceText, directive) {
  // Check if at start of string.  Doesn't need preceding whitespace.
  var regExp = new RegExp('^\\s*' + directive + ':(\\S*)\\s*', 'i');
  var matchInfo = regExp.exec(sourceText);
  if (matchInfo == null) {
    // Check if not at start of string.  Must be preceded by whitespace.
    regExp = new RegExp('\\s+' + directive + ':(\\S*)\\s*', 'i');
    matchInfo = regExp.exec(sourceText);
    if (matchInfo == null)
      return null;
  }

  return {'remainingText': sourceText.replace(regExp, ' ').trim(),
          'parameter': matchInfo[1]};
};

/**
 * Just like parseDirective_, except can optionally be a '-' before or
 * the parameter, to negate it.  Before is more natural, after
 * allows more convenient toggling.
 *
 * Returned value has the additional field |isNegated|, and a leading
 * '-' will be removed from |parameter|, if present.
 */
RequestsView.prototype.parseNegatableDirective_ = function(sourceText,
                                                           directive) {
  var matchInfo = this.parseDirective_(sourceText, directive);
  if (matchInfo == null)
    return null;

  // Remove any leading or trailing '-' from the directive.
  var negationInfo = /^(-?)(\S*?)$/.exec(matchInfo.parameter);
  matchInfo.parameter = negationInfo[2];
  matchInfo.isNegated = (negationInfo[1] == '-');
  return matchInfo;
};

/**
 * Parse any "sort:" directives, and update |comparisonFunction_| and
 * |doSortBackwards_|as needed.  Note only the last valid sort directive
 * is used.
 *
 * Returns |filterText| with all sort directives removed, including
 * invalid ones.
 */
RequestsView.prototype.parseSortDirectives_ = function(filterText) {
  this.comparisonFunction_ = RequestsView.compareSourceId_;
  this.doSortBackwards_ = false;

  while (true) {
    var sortInfo = this.parseNegatableDirective_(filterText, 'sort');
    if (sortInfo == null)
      break;
    var comparisonName = sortInfo.parameter.toLowerCase();
    if (RequestsView.comparisonFunctionTable_[comparisonName] != null) {
      this.comparisonFunction_ =
          RequestsView.comparisonFunctionTable_[comparisonName];
      this.doSortBackwards_ = sortInfo.isNegated;
    }
    filterText = sortInfo.remainingText;
  }

  return filterText;
};

/**
 * Parse any "is:" directives, and update |filter| accordingly.
 *
 * Returns |filterText| with all "is:" directives removed, including
 * invalid ones.
 */
RequestsView.prototype.parseRestrictDirectives_ = function(filterText, filter) {
  while (true) {
    var filterInfo = this.parseNegatableDirective_(filterText, 'is');
    if (filterInfo == null)
      break;
    if (filterInfo.parameter == 'active') {
      if (!filterInfo.isNegated)
        filter.isActive = true;
      else
        filter.isInactive = true;
    }
    filterText = filterInfo.remainingText;
  }
  return filterText;
};

RequestsView.prototype.setFilter_ = function(filterText) {
  var lastComparisonFunction = this.comparisonFunction_;
  var lastDoSortBackwards = this.doSortBackwards_;

  filterText = this.parseSortDirectives_(filterText);

  if (lastComparisonFunction != this.comparisonFunction_ ||
      lastDoSortBackwards != this.doSortBackwards_) {
    this.Sort_();
  }

  this.currentFilter_ = {};
  filterText = this.parseRestrictDirectives_(filterText, this.currentFilter_);
  this.currentFilter_.text = filterText.trim();

  // Iterate through all of the rows and see if they match the filter.
  for (var id in this.sourceIdToEntryMap_) {
    var entry = this.sourceIdToEntryMap_[id];
    entry.setIsMatchedByFilter(entry.matchesFilter(this.currentFilter_));
  }
};

/**
 * Repositions |sourceEntry|'s row in the table using an insertion sort.
 * Significantly faster than sorting the entire table again, when only
 * one entry has changed.
 */
RequestsView.prototype.InsertionSort_ = function(sourceEntry) {
  // SourceEntry that should be after |sourceEntry|, if it needs
  // to be moved earlier in the list.
  var sourceEntryAfter = sourceEntry;
  while (true) {
    var prevSourceId = sourceEntryAfter.getPreviousNodeSourceId();
    if (prevSourceId == null)
      break;
    var prevSourceEntry = this.sourceIdToEntryMap_[prevSourceId];
    if (this.comparisonFuncWithReversing_(sourceEntry, prevSourceEntry) >= 0)
      break;
    sourceEntryAfter = prevSourceEntry;
  }
  if (sourceEntryAfter != sourceEntry) {
    sourceEntry.moveBefore(sourceEntryAfter);
    return;
  }

  var sourceEntryBefore = sourceEntry;
  while (true) {
    var nextSourceId = sourceEntryBefore.getNextNodeSourceId();
    if (nextSourceId == null)
      break;
    var nextSourceEntry = this.sourceIdToEntryMap_[nextSourceId];
    if (this.comparisonFuncWithReversing_(sourceEntry, nextSourceEntry) <= 0)
      break;
    sourceEntryBefore = nextSourceEntry;
  }
  if (sourceEntryBefore != sourceEntry)
    sourceEntry.moveAfter(sourceEntryBefore);
};

RequestsView.prototype.onLogEntryAdded = function(logEntry) {
  var id = logEntry.source.id;

  // Lookup the source.
  var sourceEntry = this.sourceIdToEntryMap_[id];

  if (!sourceEntry) {
    sourceEntry = new SourceEntry(this, this.maxReceivedSourceId_);
    this.sourceIdToEntryMap_[id] = sourceEntry;
    this.incrementPrefilterCount(1);
    if (id > this.maxReceivedSourceId_)
      this.maxReceivedSourceId_ = id;
  }

  sourceEntry.update(logEntry);

  if (sourceEntry.isSelected())
    this.invalidateDetailsView_();

  // TODO(mmenke): Fix sorting when sorting by duration.
  //               Duration continuously increases for all entries that are
  //               still active.  This can result in incorrect sorting, until
  //               Sort_ is called.
  this.InsertionSort_(sourceEntry);
};

/**
 * Called whenever some log events are deleted.  |sourceIds| lists
 * the source IDs of all deleted log entries.
 */
RequestsView.prototype.onLogEntriesDeleted = function(sourceIds) {
  for (var i = 0; i < sourceIds.length; ++i) {
    var id = sourceIds[i];
    var entry = this.sourceIdToEntryMap_[id];
    if (entry) {
      entry.remove();
      delete this.sourceIdToEntryMap_[id];
      this.incrementPrefilterCount(-1);
    }
  }
};

/**
 * Called whenever all log events are deleted.
 */
RequestsView.prototype.onAllLogEntriesDeleted = function(offset) {
  this.initializeSourceList_();
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
  var sourceIds = [];
  for (var i = 0; i < this.currentSelectedSources_.length; ++i) {
    var entry = this.currentSelectedSources_[i];
    sourceIds.push(entry.getSourceId());
  }
  g_browser.deleteEventsBySourceId(sourceIds);
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

/**
 * If already using the specified sort method, flips direction.  Otherwise,
 * removes pre-existing sort parameter before adding the new one.
 */
RequestsView.prototype.toggleSortMethod_ = function(sortMethod) {
  // Remove old sort directives, if any.
  var filterText = this.parseSortDirectives_(this.getFilterText_());

  // If already using specified sortMethod, sort backwards.
  if (!this.doSortBackwards_ &&
      RequestsView.comparisonFunctionTable_[sortMethod] ==
          this.comparisonFunction_)
    sortMethod = '-' + sortMethod;

  filterText = 'sort:' + sortMethod + ' ' + filterText;
  this.setFilterText_(filterText.trim());
};

RequestsView.prototype.sortById_ = function(event) {
  this.toggleSortMethod_('id');
};

RequestsView.prototype.sortBySourceType_ = function(event) {
  this.toggleSortMethod_('source');
};

RequestsView.prototype.sortByDescription_ = function(event) {
  this.toggleSortMethod_('desc');
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
};

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
