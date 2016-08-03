// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-list-container',

  properties: {
    // The path of the currently selected page.
    selectedPage_: String,

    // Whether domain-grouped history is enabled.
    grouped: Boolean,

    /** @type {!QueryState} */
    queryState: Object,

    /** @type {!QueryResult} */
    queryResult: Object,
  },

  observers: [
    'groupedRangeChanged_(queryState.range)',
  ],

  listeners: {
    'history-list-scrolled': 'closeMenu_',
    'load-more-history': 'loadMoreHistory_',
    'toggle-menu': 'toggleMenu_',
  },

  /**
   * @param {HistoryQuery} info An object containing information about the
   *    query.
   * @param {!Array<HistoryEntry>} results A list of results.
   */
  historyResult: function(info, results) {
    this.initializeResults_(info, results);
    this.closeMenu_();

    if (this.selectedPage_ == 'grouped-list') {
      this.$$('#grouped-list').historyData = results;
      return;
    }

    var list = /** @type {HistoryListElement} */(this.$['infinite-list']);
    list.addNewResults(results);
    if (info.finished)
      list.disableResultLoading();
  },

  /**
   * Queries the history backend for results based on queryState.
   * @param {boolean} incremental Whether the new query should continue where
   *    the previous query stopped.
   */
  queryHistory: function(incremental) {
    var queryState = this.queryState;
    // Disable querying until the first set of results have been returned. If
    // there is a search, query immediately to support search query params from
    // the URL.
    var noResults = !this.queryResult || this.queryResult.results == null;
    if (queryState.queryingDisabled ||
        (!this.queryState.searchTerm && noResults)) {
      return;
    }

    // Close any open dialog if a new query is initiated.
    if (!incremental && this.$.dialog.open)
      this.$.dialog.close();

    this.set('queryState.querying', true);
    this.set('queryState.incremental', incremental);

    var lastVisitTime = 0;
    if (incremental) {
      var lastVisit = this.queryResult.results.slice(-1)[0];
      lastVisitTime = lastVisit ? lastVisit.time : 0;
    }

    var maxResults =
      queryState.range == HistoryRange.ALL_TIME ? RESULTS_PER_PAGE : 0;
    chrome.send('queryHistory', [
      queryState.searchTerm, queryState.groupedOffset, queryState.range,
      lastVisitTime, maxResults
    ]);
  },

  unselectAllItems: function(count) {
    /** @type {HistoryListElement} */ (this.$['infinite-list'])
        .unselectAllItems(count);
  },

  /**
   * Delete all the currently selected history items. Will prompt the user with
   * a dialog to confirm that the deletion should be performed.
   */
  deleteSelectedWithPrompt: function() {
    if (!loadTimeData.getBoolean('allowDeletingHistory'))
      return;

    this.$.dialog.showModal();
  },

  /**
   * @param {HistoryRange} range
   * @private
   */
  groupedRangeChanged_: function(range) {
    this.selectedPage_ = this.queryState.range == HistoryRange.ALL_TIME ?
      'infinite-list' : 'grouped-list';

    this.queryHistory(false);
  },

  /** @private */
  loadMoreHistory_: function() { this.queryHistory(true); },

  /**
   * @param {HistoryQuery} info
   * @param {!Array<HistoryEntry>} results
   * @private
   */
  initializeResults_: function(info, results) {
    if (results.length == 0)
      return;

    var currentDate = results[0].dateRelativeDay;

    for (var i = 0; i < results.length; i++) {
      // Sets the default values for these fields to prevent undefined types.
      results[i].selected = false;
      results[i].readableTimestamp =
          info.term == '' ? results[i].dateTimeOfDay : results[i].dateShort;

      if (results[i].dateRelativeDay != currentDate) {
        currentDate = results[i].dateRelativeDay;
      }
    }
  },

  /** @private */
  onDialogConfirmTap_: function() {
    this.$['infinite-list'].deleteSelected();
    this.$.dialog.close();
  },

  /** @private */
  onDialogCancelTap_: function() {
    this.$.dialog.close();
  },

  /**
   * Closes the overflow menu.
   * @private
   */
  closeMenu_: function() {
    /** @type {CrSharedMenuElement} */(this.$.sharedMenu).closeMenu();
  },

  /**
   * Opens the overflow menu unless the menu is already open and the same button
   * is pressed.
   * @param {{detail: {item: !HistoryEntry, target: !HTMLElement}}} e
   * @private
   */
  toggleMenu_: function(e) {
    var target = e.detail.target;
    /** @type {CrSharedMenuElement} */(this.$.sharedMenu).toggleMenu(
        target, e.detail.item);
  },

  /** @private */
  onMoreFromSiteTap_: function() {
    var menu = /** @type {CrSharedMenuElement} */(this.$.sharedMenu);
    this.fire('search-domain', {domain: menu.itemData.domain});
    menu.closeMenu();
  },

  /** @private */
  onRemoveFromHistoryTap_: function() {
    var menu = /** @type {CrSharedMenuElement} */(this.$.sharedMenu);
    md_history.BrowserService.getInstance()
        .deleteItems([menu.itemData])
        .then(function(items) {
          this.$['infinite-list'].removeDeletedHistory_(items);
          // This unselect-all is to reset the toolbar when deleting a selected
          // item. TODO(tsergeant): Make this automatic based on observing list
          // modifications.
          this.fire('unselect-all');
        }.bind(this));
    menu.closeMenu();
  },
});
