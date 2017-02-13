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
          searchTerm: '',
        };
      },
    },

    /** @type {QueryResult} */
    queryResult: Object,

    /** @type {?HistoryRouterElement} */
    router: Object,
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

    if (queryState.queryingDisabled)
      return;

    this.set('queryState.querying', true);
    this.set('queryState.incremental', incremental);

    var lastVisitTime = 0;
    if (incremental) {
      var lastVisit = this.queryResult.results.slice(-1)[0];
      lastVisitTime = lastVisit ? Math.floor(lastVisit.time) : 0;
    }

    chrome.send('queryHistory', [
      queryState.searchTerm,
      0,  // No grouped offset.
      0,  // Disable grouping.
      lastVisitTime,
      RESULTS_PER_PAGE,
    ]);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onChangeQuery_: function(e) {
    var changes = /** @type {{search: ?string}} */ (e.detail);
    var needsUpdate = false;

    if (changes.search != null &&
        changes.search != this.queryState.searchTerm) {
      this.set('queryState.searchTerm', changes.search);
      needsUpdate = true;
    }

    if (needsUpdate) {
      this.queryHistory_(false);
      if (this.router)
        this.router.serializeUrl();
    }
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
