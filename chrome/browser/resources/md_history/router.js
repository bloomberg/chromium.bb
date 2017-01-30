// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-router',

  properties: {
    selectedPage: {
      type: String,
      notify: true,
      observer: 'selectedPageChanged_'
    },

    /** @type {QueryState} */
    queryState: Object,

    grouped: Boolean,

    path_: String,

    queryParams_: Object,
  },

  /** @private {boolean} */
  parsing_: false,

  observers: [
    'onUrlChanged_(path_, queryParams_)',
  ],

  /** @override */
  attached: function() {
    // Redirect legacy search URLs to URLs compatible with material history.
    if (window.location.hash) {
      window.location.href = window.location.href.split('#')[0] + '?' +
          window.location.hash.substr(1);
    }
  },

  /**
   * Write all relevant page state to the URL.
   */
  serializeUrl: function() {
    var path = this.selectedPage;

    if (path == 'history' && this.queryState.range != HistoryRange.ALL_TIME)
      path += '/' + this.rangeToString_(this.queryState.range);

    if (path == 'history')
      path = '';

    var offsetParam = null;
    if (this.selectedPage == 'history' && this.queryState.groupedOffset)
      offsetParam = this.queryState.groupedOffset;

    // Make all modifications at the end of the method so observers can't change
    // the outcome.
    this.path_ = '/' + path;
    this.set('queryParams_.offset', offsetParam);
    this.set('queryParams_.q', this.queryState.searchTerm || null);
  },

  /** @private */
  selectedPageChanged_: function() {
    // Update the URL if the page was changed externally, but ignore the update
    // if it came from parseUrl_().
    if (!this.parsing_)
      this.serializeUrl();
  },

  /** @private */
  parseUrl_: function() {
    this.parsing_ = true;
    var changes = {};
    var sections = this.path_.substr(1).split('/');
    var page = sections[0] || 'history';

    if (page == 'history' && this.grouped) {
      var range = sections.length > 1 ? this.stringToRange_(sections[1]) :
                                        HistoryRange.ALL_TIME;
      changes.range = range;
      changes.offset = Number(this.queryParams_.offset) || 0;
    }

    changes.search = this.queryParams_.q || '';

    // Must change selectedPage before `change-query`, otherwise the
    // query-manager will call serializeUrl() with the old page.
    this.selectedPage = page;
    this.fire('change-query', changes);
    this.serializeUrl();

    this.parsing_ = false;
  },

  /** @private */
  onUrlChanged_: function() {
    // Changing the url and query parameters at the same time will cause two
    // calls to onUrlChanged_. Debounce the actual work so that these two
    // changes get processed together.
    this.debounce('parseUrl', this.parseUrl_.bind(this));
  },

  /**
   * @param {!HistoryRange} range
   * @return {string}
   */
  rangeToString_: function(range) {
    switch (range) {
      case HistoryRange.WEEK:
        return 'week';
      case HistoryRange.MONTH:
        return 'month';
      default:
        return '';
    }
  },

  /**
   * @param {string} str
   * @return {HistoryRange}
   */
  stringToRange_: function(str) {
    switch (str) {
      case 'week':
        return HistoryRange.WEEK;
      case 'month':
        return HistoryRange.MONTH;
      default:
        return HistoryRange.ALL_TIME;
    }
  }
});
