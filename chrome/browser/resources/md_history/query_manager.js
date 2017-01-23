// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-query-manager',

  properties: {
    /** @type {QueryState} */
    queryState: {
      type: Object,
      notify: true,
    },

    groupedRange_: {
      type: Number,
      // Use a computed property to be able to compare old and new values.
      computed: 'computeGroupedRange_(queryState.range)',
    },

    /** @type {QueryResult} */
    queryResult: Object,
  },

  observers: [
    'searchTermChanged_(queryState.searchTerm)',
    'groupedOffsetChanged_(queryState.groupedOffset)',
  ],

  /** @private {?function(!Event)} */
  boundOnQueryHistory_: null,

  /** @override */
  attached: function() {
    this.boundOnQueryHistory_ = this.onQueryHistory_.bind(this);
    document.addEventListener('query-history', this.boundOnQueryHistory_);
  },

  /** @override */
  detached: function() {
    document.removeEventListener('query-history', this.boundOnQueryHistory_);
  },

  /**
   * @param {boolean} incremental
   * @private
   */
  queryHistory_: function(incremental) {
    var queryState = this.queryState;
    // Disable querying until the first set of results have been returned. If
    // there is a search, query immediately to support search query params from
    // the URL.
    var noResults = !this.queryResult || this.queryResult.results == null;
    if (queryState.queryingDisabled ||
        (!this.queryState.searchTerm && noResults)) {
      return;
    }

    this.set('queryState.querying', true);
    this.set('queryState.incremental', incremental);

    var lastVisitTime = 0;
    if (incremental) {
      var lastVisit = this.queryResult.results.slice(-1)[0];
      lastVisitTime = lastVisit ? Math.floor(lastVisit.time) : 0;
    }

    var maxResults =
        this.queryState.range == HistoryRange.ALL_TIME ? RESULTS_PER_PAGE : 0;

    chrome.send('queryHistory', [
      queryState.searchTerm,
      queryState.groupedOffset,
      queryState.range,
      lastVisitTime,
      maxResults,
    ]);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onQueryHistory_: function(e) {
    this.queryHistory_(/** @type {boolean} */ e.detail);
    return false;
  },

  /** @private */
  groupedOffsetChanged_: function() {
    this.queryHistory_(false);
  },

  /**
   * @param {HistoryRange} range
   * @return {HistoryRange}
   * @private
   */
  computeGroupedRange_: function(range) {
    if (this.groupedRange_ != undefined) {
      this.set('queryState.groupedOffset', 0);

      this.queryHistory_(false);
      this.fire('history-view-changed');
    }

    return range;
  },

  /** @private */
  searchTermChanged_: function() {
    this.queryHistory_(false);
    // TODO(tsergeant): Ignore incremental searches in this metric.
    if (this.queryState.searchTerm)
      md_history.BrowserService.getInstance().recordAction('Search');
  },
});
