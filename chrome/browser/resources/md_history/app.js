// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-app',

  properties: {
    // The id of the currently selected page.
    selectedPage: {
      type: String,
      value: 'history-list'
    }
  },

  // TODO(calamity): Replace these event listeners with data bound properties.
  listeners: {
    'history-checkbox-select': 'checkboxSelected',
    'unselect-all': 'unselectAll',
    'delete-selected': 'deleteSelected',
    'search-changed': 'searchChanged',
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

    var historyList =
        /** @type {HistoryListElement} */(this.$['history-list']);
    var toolbar = /** @type {HistoryToolbarElement} */(this.$.toolbar);
    var toBeRemoved = historyList.getSelectedItems(toolbar.count);
    chrome.send('removeVisits', toBeRemoved);
  },

  /**
   * When the search is changed refresh the results from the backend.
   * Ensures that the search bar is updated with the new search term.
   * @param {{detail: {search: string}}} e
   */
  searchChanged: function(e) {
    this.$.toolbar.setSearchTerm(e.detail.search);
    /** @type {HistoryListElement} */(this.$['history-list']).setLoading();
    /** @type {HistoryToolbarElement} */(this.$.toolbar).searching = true;
    chrome.send('queryHistory', [e.detail.search, 0, 0, 0, RESULTS_PER_PAGE]);
  },

  /**
   * @param {HistoryQuery} info An object containing information about the
   *    query.
   * @param {!Array<HistoryEntry>} results A list of results.
   */
  historyResult: function(info, results) {
    var list = /** @type {HistoryListElement} */(this.$['history-list']);
    list.addNewResults(results, info.term);
    /** @type {HistoryToolbarElement} */(this.$.toolbar).searching = false;
    if (info.finished)
      list.disableResultLoading();
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
  },

  deleteComplete: function() {
    var historyList = /** @type {HistoryListElement} */(this.$['history-list']);
    var toolbar = /** @type {HistoryToolbarElement} */(this.$.toolbar);
    historyList.removeDeletedHistory(toolbar.count);
    toolbar.count = 0;
  }
});
