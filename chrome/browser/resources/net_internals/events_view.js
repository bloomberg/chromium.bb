// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
 *  |                      ||                |
 *  |                      ||                |
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
    var leftPane = new VerticalSplitView(new DivView(EventsView.TOPBAR_ID),
                                         new DivView(EventsView.LIST_BOX_ID));

    this.detailsView_ = new DetailsView(EventsView.DETAILS_LOG_BOX_ID);

    this.splitterView_ = new ResizableVerticalSplitView(
        leftPane, this.detailsView_, new DivView(EventsView.SIZER_ID));

    SourceTracker.getInstance().addSourceEntryObserver(this);

    this.tableBody_ = $(EventsView.TBODY_ID);

    this.filterInput_ = $(EventsView.FILTER_INPUT_ID);
    this.filterCount_ = $(EventsView.FILTER_COUNT_ID);

    this.filterInput_.addEventListener('search',
        this.onFilterTextChanged_.bind(this), true);

    $(EventsView.SELECT_ALL_ID).addEventListener(
        'click', this.selectAll_.bind(this), true);

    $(EventsView.SORT_BY_ID_ID).addEventListener(
        'click', this.sortById_.bind(this), true);

    $(EventsView.SORT_BY_SOURCE_TYPE_ID).addEventListener(
        'click', this.sortBySourceType_.bind(this), true);

    $(EventsView.SORT_BY_DESCRIPTION_ID).addEventListener(
        'click', this.sortByDescription_.bind(this), true);

    new MouseOverHelp(EventsView.FILTER_HELP_ID,
                      EventsView.FILTER_HELP_HOVER_ID);

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
  EventsView.FILTER_HELP_ID = 'events-view-filter-help';
  EventsView.FILTER_HELP_HOVER_ID = 'events-view-filter-help-hover';
  EventsView.SELECT_ALL_ID = 'events-view-select-all';
  EventsView.SORT_BY_ID_ID = 'events-view-sort-by-id';
  EventsView.SORT_BY_SOURCE_TYPE_ID = 'events-view-sort-by-source';
  EventsView.SORT_BY_DESCRIPTION_ID = 'events-view-sort-by-description';
  EventsView.DETAILS_LOG_BOX_ID = 'events-view-details-log-box';
  EventsView.TOPBAR_ID = 'events-view-filter-box';
  EventsView.LIST_BOX_ID = 'events-view-source-list';
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
     * Updates text in the details view when privacy stripping is toggled.
     */
    onPrivacyStrippingChanged: function() {
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

    setFilter_: function(filterText) {
      var lastComparisonFunction = this.comparisonFunction_;
      var lastDoSortBackwards = this.doSortBackwards_;

      this.pickSortFunction_(filterText);

      if (lastComparisonFunction != this.comparisonFunction_ ||
          lastDoSortBackwards != this.doSortBackwards_) {
        this.sort_();
      }

      var oldFilter = this.currentFilter_;
      this.currentFilter_ = createFilter_(filterText);

      // No need to filter again if filters match.
      if (oldFilter &&
          JSON.stringify(oldFilter) == JSON.stringify(this.currentFilter_)) {
        return;
      }

      // Iterate through all of the rows and see if they match the filter.
      for (var id in this.sourceIdToRowMap_) {
        var entry = this.sourceIdToRowMap_[id];
        entry.setIsMatchedByFilter(entry.matchesFilter(this.currentFilter_));
      }
    },

    /**
     * Parse any "sort:" directives, and update |comparisonFunction_| and
     * |doSortBackwards_| as needed.  Note only the last valid sort directive
     * is used.
     */
    pickSortFunction_: function(filterText) {
      this.comparisonFunction_ = compareSourceId_;
      this.doSortBackwards_ = false;

      var filterList = parseFilter_(filterText);
      for (var i = 0; i < filterList.length; ++i) {
        var sort = parseSortDirective_(filterList[i].parsed);
        if (sort != null) {
          this.comparisonFunction_ = sort.comparisonFunction;
          this.doSortBackwards_ = sort.backwards;
        }
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
     * all items.  If it includes a selection, tries to select the relevant
     * item.
     */
    setParameters: function(params) {
      if (params.q) {
        this.unselectAll_();
        this.setFilterText_(params.q);
      }

      if (params.s) {
        var sourceRow = this.sourceIdToRowMap_[params.s];
        if (sourceRow) {
          sourceRow.setSelected(true);
          this.scrollToSourceId(params.s);
        }
      }
    },

    /**
     * Scrolls to the source indicated by |sourceId|, if displayed.
     */
    scrollToSourceId: function(sourceId) {
      this.detailsView_.scrollToSourceId(sourceId);
    },

    /**
     * If already using the specified sort method, flips direction.  Otherwise,
     * removes pre-existing sort parameter before adding the new one.
     */
    toggleSortMethod_: function(sortMethod) {
      // Get old filter text and remove old sort directives, if any.
      var filterList = parseFilter_(this.getFilterText_());
      var filterText = '';
      for (var i = 0; i < filterList.length; ++i) {
        if (parseSortDirective_(filterList[i].parsed) == null)
          filterText += filterList[i].original;
      }

      // If already using specified sortMethod, sort backwards.
      if (!this.doSortBackwards_ &&
          COMPARISON_FUNCTION_TABLE[sortMethod] == this.comparisonFunction_) {
        sortMethod = '-' + sortMethod;
      }

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

    /**
     * Modifies the map of selected rows to include/exclude the one with
     * |sourceId|, if present.  Does not modify checkboxes or the LogView.
     * Should only be called by a SourceRow in response to its selection
     * state changing.
     */
    modifySelectionArray: function(sourceId, addToSelection) {
      var sourceRow = this.sourceIdToRowMap_[sourceId];
      if (!sourceRow)
        return;
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
      for (var i = 0; i < this.currentSelectedRows_.length; ++i) {
        sourceEntries.push(this.currentSelectedRows_[i].getSourceEntry());
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
    '': compareSourceId_,
    'active': compareActive_,
    'desc': compareDescription_,
    'description': compareDescription_,
    'duration': compareDuration_,
    'id': compareSourceId_,
    'source': compareSourceType_,
    'type': compareSourceType_
  };

  /**
   * Sorts active entries first.  If both entries are inactive, puts the one
   * that was active most recently first.  If both are active, uses source ID,
   * which puts longer lived events at the top, and behaves better than using
   * duration or time of first event.
   */
  function compareActive_(source1, source2) {
    if (!source1.isInactive() && source2.isInactive())
      return -1;
    if (source1.isInactive() && !source2.isInactive())
      return 1;
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
      return -compareSourceId_(source1, source2);
    }
    return compareSourceId_(source1, source2);
  }

  function compareDescription_(source1, source2) {
    var source1Text = source1.getDescription().toLowerCase();
    var source2Text = source2.getDescription().toLowerCase();
    var compareResult = source1Text.localeCompare(source2Text);
    if (compareResult != 0)
      return compareResult;
    return compareSourceId_(source1, source2);
  }

  function compareDuration_(source1, source2) {
    var durationDifference = source2.getDuration() - source1.getDuration();
    if (durationDifference)
      return durationDifference;
    return compareSourceId_(source1, source2);
  }

  /**
   * For the purposes of sorting by source IDs, entries without a source
   * appear right after the SourceEntry with the highest source ID received
   * before the sourceless entry. Any ambiguities are resolved by ordering
   * the entries without a source by the order in which they were received.
   */
  function compareSourceId_(source1, source2) {
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

  function compareSourceType_(source1, source2) {
    var source1Text = source1.getSourceTypeString();
    var source2Text = source2.getSourceTypeString();
    var compareResult = source1Text.localeCompare(source2Text);
    if (compareResult != 0)
      return compareResult;
    return compareSourceId_(source1, source2);
  }

  /**
   * Parses a single "sort:" directive, and returns a dictionary containing
   * the sort function and direction.  Returns null on failure, including
   * the case when no such sort function exists.
   */

  function parseSortDirective_(filterElement) {
    var match = /^sort:(-?)(.*)$/.exec(filterElement);
    if (!match || !COMPARISON_FUNCTION_TABLE[match[2]])
      return null;
    return {
      comparisonFunction: COMPARISON_FUNCTION_TABLE[match[2]],
      backwards: (match[1] == '-'),
    };
  }

  /**
   * Parses an "is:" directive, and updates |filter| accordingly.
   *
   * Returns true on success, and false if |filterElement| is not an "is:"
   * directive.
   */
  function parseRestrictDirective_(filterElement, filter) {
    var match = /^is:(-?)(.*)$/.exec(filterElement);
    if (!match)
      return false;
    if (match[2] == 'active') {
      if (match[1] == '-') {
        filter.isInactive = true;
      } else {
        filter.isActive = true;
      }
      return true;
    }
    if (match[2] == 'error') {
      if (match[1] == '-') {
        filter.isNotError = true;
      } else {
        filter.isError = true;
      }
      return true;
    }
    return false;
  }

  /**
   * Parses all directives that take arbitrary strings as input,
   * and updates |filter| accordingly.  Directives of these types
   * are stored as lists.
   *
   * Returns true on success, and false if |filterElement| is not a
   * recognized directive.
   */
  function parseStringDirective_(filterElement, filter) {
    var directives = ['type', 'id'];
    for (var i = 0; i < directives.length; ++i) {
      var directive = directives[i];
      var match = RegExp('^' + directive + ':(.*)$').exec(filterElement);
      if (!match)
        continue;

      // Split parameters around commas and remove empty elements.
      var parameters = match[1].split(',');
      parameters = parameters.filter(function(string) {
        return string.length > 0;
      });

      // If there's already a matching filter, take the intersection.
      // This behavior primarily exists for tests.  It is not correct
      // when one of the 'type' filters is a partial match.
      if (filter[directive]) {
        parameters = parameters.filter(function(string) {
          return filter[directive].indexOf(string) != -1;
        });
      }

      filter[directive] = parameters;
      return true;
    }
    return false;
  }

  /**
   * Takes in the text of a filter and returns a list of {parsed, original}
   * pairs that correspond to substrings of the filter before and after
   * filtering.  This function is used both to parse filters and to remove
   * the sort rule from a filter.  Extra whitespace other than a single
   * character after each element is ignored.  Parsed strings are all
   * lowercase.
   */
  function parseFilter_(filterText) {
    filterText = filterText.toLowerCase();

    // Assemble a list of quoted and unquoted strings in the filter.
    var filterList = [];
    var position = 0;
    while (position < filterText.length) {
      var inQuote = false;
      var filterElement = '';
      var startPosition = position;
      while (position < filterText.length) {
        var nextCharacter = filterText[position];
        ++position;
        if (nextCharacter == '\\' &&
            position < filterText.length) {
          // If there's a backslash, skip the backslash and add the next
          // character to the element.
          filterElement += filterText[position];
          ++position;
          continue;
        } else if (nextCharacter == '"') {
          // If there's an unescaped quote character, toggle |inQuote| without
          // modifying the element.
          inQuote = !inQuote;
        } else if (!inQuote && /\s/.test(nextCharacter)) {
          // If not in a quote and have a whitespace character, that's the
          // end of the element.
          break;
        } else {
          // Otherwise, add the next character to the element.
          filterElement += nextCharacter;
        }
      }

      if (filterElement.length > 0) {
        var filter = {
          parsed: filterElement,
          original: filterText.substring(startPosition, position),
        };
        filterList.push(filter);
      }
    }
    return filterList;
  }

  /**
   * Converts |filterText| into an object representing the filter.
   */
  function createFilter_(filterText) {
    var filter = {};
    var filterList = parseFilter_(filterText);

    for (var i = 0; i < filterList.length; ++i) {
      if (parseSortDirective_(filterList[i].parsed) ||
          parseRestrictDirective_(filterList[i].parsed, filter) ||
          parseStringDirective_(filterList[i].parsed, filter)) {
        continue;
      }
      if (filter.textFilters == undefined)
        filter.textFilters = [];
      filter.textFilters.push(filterList[i].parsed);
    }
    return filter;
  }

  return EventsView;
})();
