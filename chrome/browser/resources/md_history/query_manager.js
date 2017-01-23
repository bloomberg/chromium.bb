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
      value: function() {
        return {
          // Whether the most recent query was incremental.
          incremental: false,
          // A query is initiated by page load.
          querying: true,
          queryingDisabled: false,
          _range: HistoryRange.ALL_TIME,
          searchTerm: '',
          groupedOffset: 0,

          set range(val) {
            this._range = Number(val);
          },
          get range() {
            return this._range;
          },
        };
      },
    },

    /** @type {QueryResult} */
    queryResult: Object,
  },

  observers: [
    'searchTermChanged_(queryState.searchTerm)',
  ],

  /** @private {!Object<string, !function(!Event)>} */
  documentListeners_: {},

  /** @override */
  attached: function() {
    this.documentListeners_['change-query'] = this.onChangeQuery_.bind(this);
    this.documentListeners_['query-history'] = this.onQueryHistory_.bind(this);

    for (var e in this.documentListeners_)
      document.addEventListener(e, this.documentListeners_[e]);
  },

  /** @override */
  detached: function() {
    for (var e in this.documentListeners_)
      document.removeEventListener(e, this.documentListeners_[e]);
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
  onChangeQuery_: function(e) {
    var changes =
        /** @type {{range: ?HistoryRange, offset: ?number, search: ?string}} */
        (e.detail);
    var needsUpdate = false;

    if (changes.range != null && changes.range != this.queryState.range) {
      this.set('queryState.range', changes.range);
      needsUpdate = true;

      // Reset back to page 0 of the results, unless changing to a specific
      // page.
      if (!changes.offset)
        this.set('queryState.groupedOffset', 0);

      this.fire('history-view-changed');
    }

    if (changes.offset != null &&
        changes.offset != this.queryState.groupedOffset) {
      this.set('queryState.groupedOffset', changes.offset);
      needsUpdate = true;
    }

    if (changes.search != null &&
        changes.search != this.queryState.searchTerm) {
      this.set('queryState.searchTerm', changes.search);
      needsUpdate = true;
    }

    if (needsUpdate)
      this.queryHistory_(false);
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
  searchTermChanged_: function() {
    // TODO(tsergeant): Ignore incremental searches in this metric.
    if (this.queryState.searchTerm)
      md_history.BrowserService.getInstance().recordAction('Search');
  },
});
