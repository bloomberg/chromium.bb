// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * EventsView displays a filtered list of all events sharing a source, and
 * a details pane for the selected sources.
 *
 *  +----------------------++----------------+
 *  |      filter box      ||                |
 *  +----------------------+|                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |                      ||                |
 *  |     source list      ||    details     |
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
function EventsView(tableBodyId, filterInputId, filterCountId,
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

  this.filterInput_.addEventListener('search',
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

inherits(EventsView, View);

/**
 * Initializes the list of source entries.  If source entries are already,
 * being displayed, removes them all in the process.
 */
EventsView.prototype.initializeSourceList_ = function() {
  this.currentSelectedSources_ = [];
  this.sourceIdToEntryMap_ = {};
  this.tableBody_.innerHTML = '';
  this.numPrefilter_ = 0;
  this.numPostfilter_ = 0;
  this.invalidateFilterCounter_();
  this.invalidateDetailsView_();
};

// How soon after updating the filter list the counter should be updated.
EventsView.REPAINT_FILTER_COUNTER_TIMEOUT_MS = 0;

EventsView.prototype.setGeometry = function(left, top, width, height) {
  EventsView.superClass_.setGeometry.call(this, left, top, width, height);
  this.splitterView_.setGeometry(left, top, width, height);
};

EventsView.prototype.show = function(isVisible) {
  EventsView.superClass_.show.call(this, isVisible);
  this.splitterView_.show(isVisible);
};

EventsView.prototype.getFilterText_ = function() {
  return this.filterInput_.value;
};

EventsView.prototype.setFilterText_ = function(filterText) {
  this.filterInput_.value = filterText;
  this.onFilterTextChanged_();
};

EventsView.prototype.onFilterTextChanged_ = function() {
  this.setFilter_(this.getFilterText_());
};

/**
 * Sorts active entries first.   If both entries are inactive, puts the one
 * that was active most recently first.  If both are active, uses source ID,
 * which puts longer lived events at the top, and behaves better than using
 * duration or time of first event.
 */
EventsView.compareActive_ = function(source1, source2) {
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
    return -EventsView.compareSourceId_(source1, source2);
  }
  return EventsView.compareSourceId_(source1, source2);
};

EventsView.compareDescription_ = function(source1, source2) {
  var source1Text = source1.getDescription().toLowerCase();
  var source2Text = source2.getDescription().toLowerCase();
  var compareResult = source1Text.localeCompare(source2Text);
  if (compareResult != 0)
    return compareResult;
  return EventsView.compareSourceId_(source1, source2);
};

EventsView.compareDuration_ = function(source1, source2) {
  var durationDifference = source2.getDuration() - source1.getDuration();
  if (durationDifference)
    return durationDifference;
  return EventsView.compareSourceId_(source1, source2);
};

/**
 * For the purposes of sorting by source IDs, entries without a source
 * appear right after the SourceEntry with the highest source ID received
 * before the sourceless entry. Any ambiguities are resolved by ordering
 * the entries without a source by the order in which they were received.
 */
EventsView.compareSourceId_ = function(source1, source2) {
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

EventsView.compareSourceType_ = function(source1, source2) {
  var source1Text = source1.getSourceTypeString();
  var source2Text = source2.getSourceTypeString();
  var compareResult = source1Text.localeCompare(source2Text);
  if (compareResult != 0)
    return compareResult;
  return EventsView.compareSourceId_(source1, source2);
};

EventsView.prototype.comparisonFuncWithReversing_ = function(a, b) {
  var result = this.comparisonFunction_(a, b);
  if (this.doSortBackwards_)
    result *= -1;
  return result;
};

EventsView.comparisonFunctionTable_ = {
  // sort: and sort:- are allowed
  '':            EventsView.compareSourceId_,
  'active':      EventsView.compareActive_,
  'desc':        EventsView.compareDescription_,
  'description': EventsView.compareDescription_,
  'duration':    EventsView.compareDuration_,
  'id':          EventsView.compareSourceId_,
  'source':      EventsView.compareSourceType_,
  'type':        EventsView.compareSourceType_
};

EventsView.prototype.Sort_ = function() {
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
EventsView.prototype.parseDirective_ = function(sourceText, directive) {
  // Adding a leading space allows a single regexp to be used, regardless of
  // whether or not the directive is at the start of the string.
  sourceText = ' ' + sourceText;
  regExp = new RegExp('\\s+' + directive + ':(\\S*)\\s*', 'i');
  matchInfo = regExp.exec(sourceText);
  if (matchInfo == null)
    return null;

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
EventsView.prototype.parseNegatableDirective_ = function(sourceText,
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
EventsView.prototype.parseSortDirectives_ = function(filterText) {
  this.comparisonFunction_ = EventsView.compareSourceId_;
  this.doSortBackwards_ = false;

  while (true) {
    var sortInfo = this.parseNegatableDirective_(filterText, 'sort');
    if (sortInfo == null)
      break;
    var comparisonName = sortInfo.parameter.toLowerCase();
    if (EventsView.comparisonFunctionTable_[comparisonName] != null) {
      this.comparisonFunction_ =
          EventsView.comparisonFunctionTable_[comparisonName];
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
EventsView.prototype.parseRestrictDirectives_ = function(filterText, filter) {
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

/**
 * Parses all directives that take arbitrary strings as input,
 * and updates |filter| accordingly.  Directives of these types
 * are stored as lists.
 *
 * Returns |filterText| with all recognized directives removed.
 */
EventsView.prototype.parseStringDirectives_ = function(filterText, filter) {
  var directives = ['type', 'id'];
  for (var i = 0; i < directives.length; ++i) {
    while (true) {
      var directive = directives[i];
      var filterInfo = this.parseDirective_(filterText, directive);
      if (filterInfo == null)
        break;
      if (!filter[directive])
        filter[directive] = [];
      filter[directive].push(filterInfo.parameter);
      filterText = filterInfo.remainingText;
    }
  }
  return filterText;
};

/*
 * Converts |filterText| into an object representing the filter.
 */
EventsView.prototype.createFilter_ = function(filterText) {
  var filter = {};
  filterText = filterText.toLowerCase();
  filterText = this.parseRestrictDirectives_(filterText, filter);
  filterText = this.parseStringDirectives_(filterText, filter);
  filter.text = filterText.trim();
  return filter;
};

EventsView.prototype.setFilter_ = function(filterText) {
  var lastComparisonFunction = this.comparisonFunction_;
  var lastDoSortBackwards = this.doSortBackwards_;

  filterText = this.parseSortDirectives_(filterText);

  if (lastComparisonFunction != this.comparisonFunction_ ||
      lastDoSortBackwards != this.doSortBackwards_) {
    this.Sort_();
  }

  this.currentFilter_ = this.createFilter_(filterText);

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
EventsView.prototype.InsertionSort_ = function(sourceEntry) {
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

EventsView.prototype.onLogEntryAdded = function(logEntry) {
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
 * Returns the SourceEntry with the specified ID, if there is one.
 * Otherwise, returns undefined.
 */
EventsView.prototype.getSourceEntry = function(id) {
  return this.sourceIdToEntryMap_[id];
};

/**
 * Called whenever some log events are deleted.  |sourceIds| lists
 * the source IDs of all deleted log entries.
 */
EventsView.prototype.onLogEntriesDeleted = function(sourceIds) {
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
EventsView.prototype.onAllLogEntriesDeleted = function() {
  this.initializeSourceList_();
};

/**
 * Called when either a log file is loaded or when going back to actively
 * logging events.  In either case, called after clearing the old entries,
 * but before getting any new ones.
 */
EventsView.prototype.onSetIsViewingLogFile = function(isViewingLogFile) {
  // Needed to sort new sourceless entries correctly.
  this.maxReceivedSourceId_ = 0;
};

EventsView.prototype.incrementPrefilterCount = function(offset) {
  this.numPrefilter_ += offset;
  this.invalidateFilterCounter_();
};

EventsView.prototype.incrementPostfilterCount = function(offset) {
  this.numPostfilter_ += offset;
  this.invalidateFilterCounter_();
};

EventsView.prototype.onSelectionChanged = function() {
  this.invalidateDetailsView_();
};

EventsView.prototype.clearSelection = function() {
  var prevSelection = this.currentSelectedSources_;
  this.currentSelectedSources_ = [];

  // Unselect everything that is currently selected.
  for (var i = 0; i < prevSelection.length; ++i) {
    prevSelection[i].setSelected(false);
  }

  this.onSelectionChanged();
};

EventsView.prototype.deleteSelected_ = function() {
  var sourceIds = [];
  for (var i = 0; i < this.currentSelectedSources_.length; ++i) {
    var entry = this.currentSelectedSources_[i];
    sourceIds.push(entry.getSourceId());
  }
  g_browser.deleteEventsBySourceId(sourceIds);
};

EventsView.prototype.selectAll_ = function(event) {
  for (var id in this.sourceIdToEntryMap_) {
    var entry = this.sourceIdToEntryMap_[id];
    if (entry.isMatchedByFilter()) {
      entry.setSelected(true);
    }
  }
  event.preventDefault();
};

EventsView.prototype.unselectAll_ = function() {
  var entries = this.currentSelectedSources_.slice(0);
  for (var i = 0; i < entries.length; ++i) {
    entries[i].setSelected(false);
  }
};

/**
 * If |params| includes a query, replaces the current filter and unselects.
 * all items.
 */
EventsView.prototype.setParameters = function(params) {
  if (params.q) {
    this.unselectAll_();
    this.setFilterText_(params.q);
  }
};

/**
 * If already using the specified sort method, flips direction.  Otherwise,
 * removes pre-existing sort parameter before adding the new one.
 */
EventsView.prototype.toggleSortMethod_ = function(sortMethod) {
  // Remove old sort directives, if any.
  var filterText = this.parseSortDirectives_(this.getFilterText_());

  // If already using specified sortMethod, sort backwards.
  if (!this.doSortBackwards_ &&
      EventsView.comparisonFunctionTable_[sortMethod] ==
          this.comparisonFunction_)
    sortMethod = '-' + sortMethod;

  filterText = 'sort:' + sortMethod + ' ' + filterText;
  this.setFilterText_(filterText.trim());
};

EventsView.prototype.sortById_ = function(event) {
  this.toggleSortMethod_('id');
};

EventsView.prototype.sortBySourceType_ = function(event) {
  this.toggleSortMethod_('source');
};

EventsView.prototype.sortByDescription_ = function(event) {
  this.toggleSortMethod_('desc');
};

EventsView.prototype.modifySelectionArray = function(
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

EventsView.prototype.invalidateDetailsView_ = function() {
  this.detailsView_.setData(this.currentSelectedSources_);
};

EventsView.prototype.invalidateFilterCounter_ = function() {
  if (!this.outstandingRepaintFilterCounter_) {
    this.outstandingRepaintFilterCounter_ = true;
    window.setTimeout(this.repaintFilterCounter_.bind(this),
                      EventsView.REPAINT_FILTER_COUNTER_TIMEOUT_MS);
  }
};

EventsView.prototype.repaintFilterCounter_ = function() {
  this.outstandingRepaintFilterCounter_ = false;
  this.filterCount_.innerHTML = '';
  addTextNode(this.filterCount_,
              this.numPostfilter_ + ' of ' + this.numPrefilter_);
};
