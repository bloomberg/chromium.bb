// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
 */
var EventsView = (function() {
  'use strict';

  // How soon after updating the filter list the counter should be updated.
  var REPAINT_FILTER_COUNTER_TIMEOUT_MS = 0;

  // We inherit from View.
  var superClass = View;

  /*
   * @constructor
   */
  function EventsView() {
    assertFirstConstructorCall(EventsView);

    // Call superclass's constructor.
    superClass.call(this);

    // Initialize the sub-views.
    var leftPane = new TopMidBottomView(new DivView(EventsView.TOPBAR_ID),
                                        new DivView(EventsView.MIDDLE_BOX_ID),
                                        new DivView(EventsView.BOTTOM_BAR_ID));

    this.detailsView_ = new DetailsView(EventsView.DETAILS_LOG_BOX_ID);

    this.splitterView_ = new ResizableVerticalSplitView(
        leftPane, this.detailsView_, new DivView(EventsView.SIZER_ID));

    g_browser.sourceTracker.addSourceEntryObserver(this);

    this.tableBody_ = $(EventsView.TBODY_ID);

    this.filterInput_ = $(EventsView.FILTER_INPUT_ID);
    this.filterCount_ = $(EventsView.FILTER_COUNT_ID);

    this.filterInput_.addEventListener('search',
        this.onFilterTextChanged_.bind(this), true);

    $(EventsView.DELETE_SELECTED_ID).onclick = this.deleteSelected_.bind(this);

    $(EventsView.DELETE_ALL_ID).onclick =
        g_browser.sourceTracker.deleteAllSourceEntries.bind(
            g_browser.sourceTracker);

    $(EventsView.SELECT_ALL_ID).addEventListener(
        'click', this.selectAll_.bind(this), true);

    $(EventsView.SORT_BY_ID_ID).addEventListener(
        'click', this.sortById_.bind(this), true);

    $(EventsView.SORT_BY_SOURCE_TYPE_ID).addEventListener(
        'click', this.sortBySourceType_.bind(this), true);

    $(EventsView.SORT_BY_DESCRIPTION_ID).addEventListener(
        'click', this.sortByDescription_.bind(this), true);

    // Sets sort order and filter.
    this.setFilter_('');

    this.initializeSourceList_();
  }

  // ID for special HTML element in category_tabs.html
  EventsView.TAB_HANDLE_ID = 'tab-handle-events';

  // IDs for special HTML elements in events_view.html
  EventsView.TBODY_ID = 'events-view-source-list-tbody';
  EventsView.FILTER_INPUT_ID = 'events-view-filter-input';
  EventsView.FILTER_COUNT_ID = 'events-view-filter-count';
  EventsView.DELETE_SELECTED_ID = 'events-view-delete-selected';
  EventsView.DELETE_ALL_ID = 'events-view-delete-all';
  EventsView.SELECT_ALL_ID = 'events-view-select-all';
  EventsView.SORT_BY_ID_ID = 'events-view-sort-by-id';
  EventsView.SORT_BY_SOURCE_TYPE_ID = 'events-view-sort-by-source';
  EventsView.SORT_BY_DESCRIPTION_ID = 'events-view-sort-by-description';
  EventsView.DETAILS_LOG_BOX_ID = 'events-view-details-log-box';
  EventsView.TOPBAR_ID = 'events-view-filter-box';
  EventsView.MIDDLE_BOX_ID = 'events-view-source-list';
  EventsView.BOTTOM_BAR_ID = 'events-view-action-box';
  EventsView.SIZER_ID = 'events-view-splitter-box';

  cr.addSingletonGetter(EventsView);

  EventsView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Initializes the list of source entries.  If source entries are already,
     * being displayed, removes them all in the process.
     */
    initializeSourceList_: function() {
      this.currentSelectedRows_ = [];
      this.sourceIdToRowMap_ = {};
      this.tableBody_.innerHTML = '';
      this.numPrefilter_ = 0;
      this.numPostfilter_ = 0;
      this.invalidateFilterCounter_();
      this.invalidateDetailsView_();
    },

    setGeometry: function(left, top, width, height) {
      superClass.prototype.setGeometry.call(this, left, top, width, height);
      this.splitterView_.setGeometry(left, top, width, height);
    },

    show: function(isVisible) {
      superClass.prototype.show.call(this, isVisible);
      this.splitterView_.show(isVisible);
    },

    getFilterText_: function() {
      return this.filterInput_.value;
    },

    setFilterText_: function(filterText) {
      this.filterInput_.value = filterText;
      this.onFilterTextChanged_();
    },

    onFilterTextChanged_: function() {
      this.setFilter_(this.getFilterText_());
    },

    /**
     * Updates text in the details view when security stripping is toggled.
     */
    onSecurityStrippingChanged: function() {
      this.invalidateDetailsView_();
    },

    comparisonFuncWithReversing_: function(a, b) {
      var result = this.comparisonFunction_(a, b);
      if (this.doSortBackwards_)
        result *= -1;
      return result;
    },

    sort_: function() {
      var sourceEntries = [];
      for (var id in this.sourceIdToRowMap_) {
        sourceEntries.push(this.sourceIdToRowMap_[id].getSourceEntry());
      }
      sourceEntries.sort(this.comparisonFuncWithReversing_.bind(this));

      // Reposition source rows from back to front.
      for (var i = sourceEntries.length - 2; i >= 0; --i) {
        var sourceRow = this.sourceIdToRowMap_[sourceEntries[i].getSourceId()];
        var nextSourceId = sourceEntries[i + 1].getSourceId();
        if (sourceRow.getNextNodeSourceId() != nextSourceId) {
          var nextSourceRow = this.sourceIdToRowMap_[nextSourceId];
          sourceRow.moveBefore(nextSourceRow);
        }
      }
    },

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
    parseDirective_: function(sourceText, directive) {
      // Adding a leading space allows a single regexp to be used, regardless of
      // whether or not the directive is at the start of the string.
      sourceText = ' ' + sourceText;
      var regExp = new RegExp('\\s+' + directive + ':(\\S*)\\s*', 'i');
      var matchInfo = regExp.exec(sourceText);
      if (matchInfo == null)
        return null;

      return {'remainingText': sourceText.replace(regExp, ' ').trim(),
              'parameter': matchInfo[1]};
    },

    /**
     * Just like parseDirective_, except can optionally be a '-' before or
     * the parameter, to negate it.  Before is more natural, after
     * allows more convenient toggling.
     *
     * Returned value has the additional field |isNegated|, and a leading
     * '-' will be removed from |parameter|, if present.
     */
    parseNegatableDirective_: function(sourceText, directive) {
      var matchInfo = this.parseDirective_(sourceText, directive);
      if (matchInfo == null)
        return null;

      // Remove any leading or trailing '-' from the directive.
      var negationInfo = /^(-?)(\S*?)$/.exec(matchInfo.parameter);
      matchInfo.parameter = negationInfo[2];
      matchInfo.isNegated = (negationInfo[1] == '-');
      return matchInfo;
    },

    /**
     * Parse any "sort:" directives, and update |comparisonFunction_| and
     * |doSortBackwards_|as needed.  Note only the last valid sort directive
     * is used.
     *
     * Returns |filterText| with all sort directives removed, including
     * invalid ones.
     */
    parseSortDirectives_: function(filterText) {
      this.comparisonFunction_ = compareSourceId;
      this.doSortBackwards_ = false;

      while (true) {
        var sortInfo = this.parseNegatableDirective_(filterText, 'sort');
        if (sortInfo == null)
          break;
        var comparisonName = sortInfo.parameter.toLowerCase();
        if (COMPARISON_FUNCTION_TABLE[comparisonName] != null) {
          this.comparisonFunction_ = COMPARISON_FUNCTION_TABLE[comparisonName];
          this.doSortBackwards_ = sortInfo.isNegated;
        }
        filterText = sortInfo.remainingText;
      }

      return filterText;
    },

    /**
     * Parse any "is:" directives, and update |filter| accordingly.
     *
     * Returns |filterText| with all "is:" directives removed, including
     * invalid ones.
     */
    parseRestrictDirectives_: function(filterText, filter) {
      while (true) {
        var filterInfo = this.parseNegatableDirective_(filterText, 'is');
        if (filterInfo == null)
          break;
        if (filterInfo.parameter == 'active') {
          if (!filterInfo.isNegated) {
            filter.isActive = true;
          } else {
            filter.isInactive = true;
          }
        }
        if (filterInfo.parameter == 'error') {
          if (!filterInfo.isNegated) {
            filter.isError = true;
          } else {
            filter.isNotError = true;
          }
        }
        filterText = filterInfo.remainingText;
      }
      return filterText;
    },

    /**
     * Parses all directives that take arbitrary strings as input,
     * and updates |filter| accordingly.  Directives of these types
     * are stored as lists.
     *
     * Returns |filterText| with all recognized directives removed.
     */
    parseStringDirectives_: function(filterText, filter) {
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
    },

    /*
     * Converts |filterText| into an object representing the filter.
     */
    createFilter_: function(filterText) {
      var filter = {};
      filterText = filterText.toLowerCase();
      filterText = this.parseRestrictDirectives_(filterText, filter);
      filterText = this.parseStringDirectives_(filterText, filter);
      filter.text = filterText.trim();
      return filter;
    },

    setFilter_: function(filterText) {
      var lastComparisonFunction = this.comparisonFunction_;
      var lastDoSortBackwards = this.doSortBackwards_;

      filterText = this.parseSortDirectives_(filterText);

      if (lastComparisonFunction != this.comparisonFunction_ ||
          lastDoSortBackwards != this.doSortBackwards_) {
        this.sort_();
      }

      this.currentFilter_ = this.createFilter_(filterText);

      // Iterate through all of the rows and see if they match the filter.
      for (var id in this.sourceIdToRowMap_) {
        var entry = this.sourceIdToRowMap_[id];
        entry.setIsMatchedByFilter(entry.matchesFilter(this.currentFilter_));
      }
    },

    /**
     * Repositions |sourceRow|'s in the table using an insertion sort.
     * Significantly faster than sorting the entire table again, when only
     * one entry has changed.
     */
    insertionSort_: function(sourceRow) {
      // SourceRow that should be after |sourceRow|, if it needs
      // to be moved earlier in the list.
      var sourceRowAfter = sourceRow;
      while (true) {
        var prevSourceId = sourceRowAfter.getPreviousNodeSourceId();
        if (prevSourceId == null)
          break;
        var prevSourceRow = this.sourceIdToRowMap_[prevSourceId];
        if (this.comparisonFuncWithReversing_(
                sourceRow.getSourceEntry(),
                prevSourceRow.getSourceEntry()) >= 0) {
          break;
        }
        sourceRowAfter = prevSourceRow;
      }
      if (sourceRowAfter != sourceRow) {
        sourceRow.moveBefore(sourceRowAfter);
        return;
      }

      var sourceRowBefore = sourceRow;
      while (true) {
        var nextSourceId = sourceRowBefore.getNextNodeSourceId();
        if (nextSourceId == null)
          break;
        var nextSourceRow = this.sourceIdToRowMap_[nextSourceId];
        if (this.comparisonFuncWithReversing_(
                sourceRow.getSourceEntry(),
                nextSourceRow.getSourceEntry()) <= 0) {
          break;
        }
        sourceRowBefore = nextSourceRow;
      }
      if (sourceRowBefore != sourceRow)
        sourceRow.moveAfter(sourceRowBefore);
    },

    /**
     * Called whenever SourceEntries are updated with new log entries.  Updates
     * the corresponding table rows, sort order, and the details view as needed.
     */
    onSourceEntriesUpdated: function(sourceEntries) {
      var isUpdatedSourceSelected = false;
      var numNewSourceEntries = 0;

      for (var i = 0; i < sourceEntries.length; ++i) {
        var sourceEntry = sourceEntries[i];

        // Lookup the row.
        var sourceRow = this.sourceIdToRowMap_[sourceEntry.getSourceId()];

        if (!sourceRow) {
          sourceRow = new SourceRow(this, sourceEntry);
          this.sourceIdToRowMap_[sourceEntry.getSourceId()] = sourceRow;
          ++numNewSourceEntries;
        } else {
          sourceRow.onSourceUpdated();
        }

        if (sourceRow.isSelected())
          isUpdatedSourceSelected = true;

        // TODO(mmenke): Fix sorting when sorting by duration.
        //               Duration continuously increases for all entries that
        //               are still active.  This can result in incorrect
        //               sorting, until sort_ is called.
        this.insertionSort_(sourceRow);
      }

      if (isUpdatedSourceSelected)
        this.invalidateDetailsView_();
      if (numNewSourceEntries)
        this.incrementPrefilterCount(numNewSourceEntries);
    },

    /**
     * Returns the SourceRow with the specified ID, if there is one.
     * Otherwise, returns undefined.
     */
    getSourceRow: function(id) {
      return this.sourceIdToRowMap_[id];
    },

    /**
     * Called whenever some log events are deleted.  |sourceIds| lists
     * the source IDs of all deleted log entries.
     */
    onSourceEntriesDeleted: function(sourceIds) {
      for (var i = 0; i < sourceIds.length; ++i) {
        var id = sourceIds[i];
        var sourceRow = this.sourceIdToRowMap_[id];
        if (sourceRow) {
          sourceRow.remove();
          delete this.sourceIdToRowMap_[id];
          this.incrementPrefilterCount(-1);
        }
      }
    },

    /**
     * Called whenever all log events are deleted.
     */
    onAllSourceEntriesDeleted: function() {
      this.initializeSourceList_();
    },

    /**
     * Called when either a log file is loaded, after clearing the old entries,
     * but before getting any new ones.
     */
    onLoadLogStart: function() {
      // Needed to sort new sourceless entries correctly.
      this.maxReceivedSourceId_ = 0;
    },

    onLoadLogFinish: function(data) {
      return true;
    },

    incrementPrefilterCount: function(offset) {
      this.numPrefilter_ += offset;
      this.invalidateFilterCounter_();
    },

    incrementPostfilterCount: function(offset) {
      this.numPostfilter_ += offset;
      this.invalidateFilterCounter_();
    },

    onSelectionChanged: function() {
      this.invalidateDetailsView_();
    },

    clearSelection: function() {
      var prevSelection = this.currentSelectedRows_;
      this.currentSelectedRows_ = [];

      // Unselect everything that is currently selected.
      for (var i = 0; i < prevSelection.length; ++i) {
        prevSelection[i].setSelected(false);
      }

      this.onSelectionChanged();
    },

    deleteSelected_: function() {
      var sourceIds = [];
      for (var i = 0; i < this.currentSelectedRows_.length; ++i) {
        var sourceRow = this.currentSelectedRows_[i];
        sourceIds.push(sourceRow.getSourceEntry().getSourceId());
      }
      g_browser.sourceTracker.deleteSourceEntries(sourceIds);
    },

    selectAll_: function(event) {
      for (var id in this.sourceIdToRowMap_) {
        var sourceRow = this.sourceIdToRowMap_[id];
        if (sourceRow.isMatchedByFilter()) {
          sourceRow.setSelected(true);
        }
      }
      event.preventDefault();
    },

    unselectAll_: function() {
      var entries = this.currentSelectedRows_.slice(0);
      for (var i = 0; i < entries.length; ++i) {
        entries[i].setSelected(false);
      }
    },

    /**
     * If |params| includes a query, replaces the current filter and unselects.
     * all items.
     */
    setParameters: function(params) {
      if (params.q) {
        this.unselectAll_();
        this.setFilterText_(params.q);
      }
    },

    /**
     * If already using the specified sort method, flips direction.  Otherwise,
     * removes pre-existing sort parameter before adding the new one.
     */
    toggleSortMethod_: function(sortMethod) {
      // Remove old sort directives, if any.
      var filterText = this.parseSortDirectives_(this.getFilterText_());

      // If already using specified sortMethod, sort backwards.
      if (!this.doSortBackwards_ &&
          COMPARISON_FUNCTION_TABLE[sortMethod] == this.comparisonFunction_)
        sortMethod = '-' + sortMethod;

      filterText = 'sort:' + sortMethod + ' ' + filterText;
      this.setFilterText_(filterText.trim());
    },

    sortById_: function(event) {
      this.toggleSortMethod_('id');
    },

    sortBySourceType_: function(event) {
      this.toggleSortMethod_('source');
    },

    sortByDescription_: function(event) {
      this.toggleSortMethod_('desc');
    },

    modifySelectionArray: function(sourceRow, addToSelection) {
      // Find the index for |sourceEntry| in the current selection list.
      var index = -1;
      for (var i = 0; i < this.currentSelectedRows_.length; ++i) {
        if (this.currentSelectedRows_[i] == sourceRow) {
          index = i;
          break;
        }
      }

      if (index != -1 && !addToSelection) {
        // Remove from the selection.
        this.currentSelectedRows_.splice(index, 1);
      }

      if (index == -1 && addToSelection) {
        this.currentSelectedRows_.push(sourceRow);
      }
    },

    getSelectedSourceEntries_: function() {
      var sourceEntries = [];
      for (var id in this.currentSelectedRows_) {
        sourceEntries.push(this.currentSelectedRows_[id].getSourceEntry());
      }
      return sourceEntries;
    },

    invalidateDetailsView_: function() {
      this.detailsView_.setData(this.getSelectedSourceEntries_());
    },

    invalidateFilterCounter_: function() {
      if (!this.outstandingRepaintFilterCounter_) {
        this.outstandingRepaintFilterCounter_ = true;
        window.setTimeout(this.repaintFilterCounter_.bind(this),
                          REPAINT_FILTER_COUNTER_TIMEOUT_MS);
      }
    },

    repaintFilterCounter_: function() {
      this.outstandingRepaintFilterCounter_ = false;
      this.filterCount_.innerHTML = '';
      addTextNode(this.filterCount_,
                  this.numPostfilter_ + ' of ' + this.numPrefilter_);
    }
  };  // end of prototype.

  // ------------------------------------------------------------------------
  // Helper code for comparisons
  // ------------------------------------------------------------------------

  var COMPARISON_FUNCTION_TABLE = {
    // sort: and sort:- are allowed
    '': compareSourceId,
    'active': compareActive,
    'desc': compareDescription,
    'description': compareDescription,
    'duration': compareDuration,
    'id': compareSourceId,
    'source': compareSourceType,
    'type': compareSourceType
  };

  /**
   * Sorts active entries first.  If both entries are inactive, puts the one
   * that was active most recently first.  If both are active, uses source ID,
   * which puts longer lived events at the top, and behaves better than using
   * duration or time of first event.
   */
  function compareActive(source1, source2) {
    if (!source1.isInactive() && source2.isInactive())
      return -1;
    if (source1.isInactive() && !source2.isInactive())
      return  1;
    if (source1.isInactive()) {
      var deltaEndTime = source1.getEndTime() - source2.getEndTime();
      if (deltaEndTime != 0) {
        // The one that ended most recently (Highest end time) should be sorted
        // first.
        return -deltaEndTime;
      }
      // If both ended at the same time, then odds are they were related events,
      // started one after another, so sort in the opposite order of their
      // source IDs to get a more intuitive ordering.
      return -compareSourceId(source1, source2);
    }
    return compareSourceId(source1, source2);
  }

  function compareDescription(source1, source2) {
    var source1Text = source1.getDescription().toLowerCase();
    var source2Text = source2.getDescription().toLowerCase();
    var compareResult = source1Text.localeCompare(source2Text);
    if (compareResult != 0)
      return compareResult;
    return compareSourceId(source1, source2);
  }

  function compareDuration(source1, source2) {
    var durationDifference = source2.getDuration() - source1.getDuration();
    if (durationDifference)
      return durationDifference;
    return compareSourceId(source1, source2);
  }

  /**
   * For the purposes of sorting by source IDs, entries without a source
   * appear right after the SourceEntry with the highest source ID received
   * before the sourceless entry. Any ambiguities are resolved by ordering
   * the entries without a source by the order in which they were received.
   */
  function compareSourceId(source1, source2) {
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
  }

  function compareSourceType(source1, source2) {
    var source1Text = source1.getSourceTypeString();
    var source2Text = source2.getSourceTypeString();
    var compareResult = source1Text.localeCompare(source2Text);
    if (compareResult != 0)
      return compareResult;
    return compareSourceId(source1, source2);
  }

  return EventsView;
})();
