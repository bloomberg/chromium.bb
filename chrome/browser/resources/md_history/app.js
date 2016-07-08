// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-app',

  properties: {
    // The id of the currently selected page.
    selectedPage_: {type: String, value: 'history', observer: 'unselectAll'},

    // Whether domain-grouped history is enabled.
    grouped_: {type: Boolean, reflectToAttribute: true},

    /** @type {!QueryState} */
    queryState_: {
      type: Object,
      value: function() {
        return {
          // Whether the most recent query was incremental.
          incremental: false,
          // A query is initiated by page load.
          querying: true,
          queryingDisabled: false,
          _range: HistoryRange.ALL_TIME,
          searchTerm: '',
          // TODO(calamity): Make history toolbar buttons change the offset
          groupedOffset: 0,

          set range(val) { this._range = Number(val); },
          get range() { return this._range; },
        };
      }
    },

   /** @type {!QueryResult} */
    queryResult_: {
      type: Object,
      value: function() {
        return {
          info: null,
          results: null,
          sessionList: null,
        };
      }
    },
  },

  listeners: {
    'cr-menu-tap': 'onMenuTap_',
    'history-checkbox-select': 'checkboxSelected',
    'unselect-all': 'unselectAll',
    'delete-selected': 'deleteSelected',
    'search-domain': 'searchDomain_',
  },

  /** @override */
  ready: function() {
    this.grouped_ = loadTimeData.getBoolean('groupByDomain');

    cr.ui.decorate('command', cr.ui.Command);
    document.addEventListener('canExecute', this.onCanExecute_.bind(this));
    document.addEventListener('command', this.onCommand_.bind(this));
  },

  /** @private */
  onMenuTap_: function() { this.$['side-bar'].toggle(); },

  /**
   * Listens for history-item being selected or deselected (through checkbox)
   * and changes the view of the top toolbar.
   * @param {{detail: {countAddition: number}}} e
   */
  checkboxSelected: function(e) {
    var toolbar = /** @type {HistoryToolbarElement} */ (this.$.toolbar);
    toolbar.count += e.detail.countAddition;
  },

  /**
   * Listens for call to cancel selection and loops through all items to set
   * checkbox to be unselected.
   * @private
   */
  unselectAll: function() {
    var listContainer =
        /** @type {HistoryListContainerElement} */ (this.$['history']);
    var toolbar = /** @type {HistoryToolbarElement} */ (this.$.toolbar);
    listContainer.unselectAllItems(toolbar.count);
    toolbar.count = 0;
  },

  deleteSelected: function() {
    this.$.history.deleteSelectedWithPrompt();
  },

  /**
   * @param {HistoryQuery} info An object containing information about the
   *    query.
   * @param {!Array<HistoryEntry>} results A list of results.
   */
  historyResult: function(info, results) {
    this.set('queryState_.querying', false);
    this.set('queryResult_.info', info);
    this.set('queryResult_.results', results);
    var listContainer =
        /** @type {HistoryListContainerElement} */ (this.$['history']);
    listContainer.historyResult(info, results);
  },

  /**
   * Fired when the user presses 'More from this site'.
   * @param {{detail: {domain: string}}} e
   */
  searchDomain_: function(e) { this.$.toolbar.setSearchTerm(e.detail.domain); },

  /**
   * @param {Event} e
   * @private
   */
  onCanExecute_: function(e) {
    e.canExecute = true;
  },

  /**
   * @param {Event} e
   * @private
   */
  onCommand_: function(e) {
    if (e.command.id == 'find-command')
      this.$.toolbar.showSearchField();
  },

  /**
   * @param {!Array<!ForeignSession>} sessionList Array of objects describing
   *     the sessions from other devices.
   * @param {boolean} isTabSyncEnabled Is tab sync enabled for this profile?
   */
  setForeignSessions: function(sessionList, isTabSyncEnabled) {
    if (!isTabSyncEnabled)
      return;

    this.set('queryResult_.sessionList', sessionList);
  },

  /**
   * Update sign in state of synced device manager after user logs in or out.
   * @param {boolean} isUserSignedIn
   */
  updateSignInState: function(isUserSignedIn) {
    var syncedDeviceManagerElem =
      /** @type {HistorySyncedDeviceManagerElement} */this
          .$$('history-synced-device-manager');
    syncedDeviceManagerElem.updateSignInState(isUserSignedIn);
  },

  /**
   * @param {string} selectedPage
   * @return {boolean}
   * @private
   */
  syncedTabsSelected_: function(selectedPage) {
    return selectedPage == 'synced-devices';
  },

  /**
   * @param {boolean} querying
   * @param {boolean} incremental
   * @param {string} searchTerm
   * @return {boolean} Whether a loading spinner should be shown (implies the
   *     backend is querying a new search term).
   * @private
   */
  shouldShowSpinner_: function(querying, incremental, searchTerm) {
    return querying && !incremental && searchTerm != '';
  },
});
