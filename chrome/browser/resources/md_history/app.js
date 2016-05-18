// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{querying: boolean,
 *            searchTerm: string,
 *            results: ?Array<!HistoryEntry>,
 *            info: ?HistoryQuery}}
 */
var QueryState;

Polymer({
  is: 'history-app',

  properties: {
    // The id of the currently selected page.
    selectedPage: {
      type: String,
      value: 'history-list'
    },

    /** @type {!QueryState} */
    queryState_: {
      type: Object,
      value: function() {
        return {
          // A query is initiated by page load.
          querying: true,
          searchTerm: '',
          results: null,
          info: null,
        };
      }
    },
  },

  observers: [
    'searchTermChanged_(queryState_.searchTerm)',
  ],

  // TODO(calamity): Replace these event listeners with data bound properties.
  listeners: {
    'history-checkbox-select': 'checkboxSelected',
    'unselect-all': 'unselectAll',
    'delete-selected': 'deleteSelected',
    'search-domain': 'searchDomain_',
    'load-more-history': 'loadMoreHistory_',
  },

  ready: function() {
    this.$.toolbar.isGroupedMode = loadTimeData.getBoolean('groupByDomain');
  },

  /**
   * Listens for history-item being selected or deselected (through checkbox)
   * and changes the view of the top toolbar.
   * @param {{detail: {countAddition: number}}} e
   */
  checkboxSelected: function(e) {
    var toolbar = /** @type {HistoryToolbarElement} */(this.$.toolbar);
    toolbar.count += e.detail.countAddition;
  },

  /**
   * Listens for call to cancel selection and loops through all items to set
   * checkbox to be unselected.
   */
  unselectAll: function() {
    var historyList =
        /** @type {HistoryListElement} */(this.$['history-list']);
    var toolbar = /** @type {HistoryToolbarElement} */(this.$.toolbar);
    historyList.unselectAllItems(toolbar.count);
    toolbar.count = 0;
  },

  /**
   * Listens for call to delete all selected items and loops through all items
   * to determine which ones are selected and deletes these.
   */
  deleteSelected: function() {
    if (!loadTimeData.getBoolean('allowDeletingHistory'))
      return;

    // TODO(hsampson): add a popup to check whether the user definitely
    // wants to delete the selected items.
    /** @type {HistoryListElement} */(this.$['history-list']).deleteSelected();
  },

  loadMoreHistory_: function() {
    this.queryHistory(true);
  },

  /**
   * @param {HistoryQuery} info An object containing information about the
   *    query.
   * @param {!Array<HistoryEntry>} results A list of results.
   */
  historyResult: function(info, results) {
    this.set('queryState_.querying', false);
    this.set('queryState_.results', results);
    this.set('queryState_.info', info);

    var list = /** @type {HistoryListElement} */(this.$['history-list']);
    list.addNewResults(results);
    if (info.finished)
      list.disableResultLoading();
  },

  /**
   * Fired when the user presses 'More from this site'.
   * @param {{detail: {domain: string}}} e
   */
  searchDomain_: function(e) {
    this.$.toolbar.setSearchTerm(e.detail.domain);
  },

  searchTermChanged_: function() {
    this.queryHistory(false);
  },

  queryHistory: function(incremental) {
    var lastVisitTime = 0;
    if (incremental) {
      var lastVisit = this.queryState_.results.slice(-1)[0];
      lastVisitTime = lastVisit ? lastVisit.time : 0;
    }

    this.set('queryState_.querying', true);
    chrome.send(
        'queryHistory',
        [this.queryState_.searchTerm, 0, 0, lastVisitTime, RESULTS_PER_PAGE]);
  },

  /**
   * @param {!Array<!ForeignSession>} sessionList Array of objects describing
   *     the sessions from other devices.
   * @param {boolean} isTabSyncEnabled Is tab sync enabled for this profile?
   */
  setForeignSessions: function(sessionList, isTabSyncEnabled) {
    if (!isTabSyncEnabled)
      return;

    // TODO(calamity): Add a 'no synced devices' message when sessions are
    // empty.
    var syncedDeviceElem = this.$['history-synced-device-manager'];
    var syncedDeviceManager =
        /** @type {HistorySyncedDeviceManagerElement} */(syncedDeviceElem);
    syncedDeviceManager.setSyncedHistory(sessionList);
  }
});
