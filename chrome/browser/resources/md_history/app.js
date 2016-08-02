// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-app',

  properties: {
    showSidebarFooter: Boolean,

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

    // Route data for the current page.
    routeData_: Object,

    // The query params for the page.
    queryParams_: Object,

    // True if the window is narrow enough for the page to have a drawer.
    hasDrawer_: Boolean,
  },

  observers: [
    // routeData_.page <=> selectedPage
    'routeDataChanged_(routeData_.page)',
    'selectedPageChanged_(selectedPage_)',

    // queryParams_.q <=> queryState.searchTerm
    'searchTermChanged_(queryState_.searchTerm)',
    'searchQueryParamChanged_(queryParams_.q)',

  ],

  // TODO(calamity): Replace these event listeners with data bound properties.
  listeners: {
    'cr-menu-tap': 'onMenuTap_',
    'history-checkbox-select': 'checkboxSelected',
    'unselect-all': 'unselectAll',
    'delete-selected': 'deleteSelected',
    'search-domain': 'searchDomain_',
    'history-close-drawer': 'closeDrawer_',
  },

  /** @override */
  ready: function() {
    this.grouped_ = loadTimeData.getBoolean('groupByDomain');

    cr.ui.decorate('command', cr.ui.Command);
    document.addEventListener('canExecute', this.onCanExecute_.bind(this));
    document.addEventListener('command', this.onCommand_.bind(this));

    // Redirect legacy search URLs to URLs compatible with material history.
    if (window.location.hash) {
      window.location.href = window.location.href.split('#')[0] + '?' +
          window.location.hash.substr(1);
    }
  },

  /** @private */
  onMenuTap_: function() {
    var drawer = this.$$('#drawer');
    if (drawer)
      drawer.toggle();
  },

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
    e = /** @type {cr.ui.CanExecuteEvent} */(e);
    switch (e.command.id) {
      case 'find-command':
        e.canExecute = true;
        break;
      case 'slash-command':
        e.canExecute = !this.$.toolbar.searchBar.isSearchFocused();
        break;
      case 'delete-command':
        e.canExecute = this.$.toolbar.count > 0;
        break;
    }
  },

  /**
   * @param {string} searchTerm
   * @private
   */
  searchTermChanged_: function(searchTerm) {
    this.set('queryParams_.q', searchTerm || null);
    this.$['history'].queryHistory(false);
  },

  /**
   * @param {string} searchQuery
   * @private
   */
  searchQueryParamChanged_: function(searchQuery) {
    this.$.toolbar.setSearchTerm(searchQuery || '');
  },

  /**
   * @param {Event} e
   * @private
   */
  onCommand_: function(e) {
    if (e.command.id == 'find-command' || e.command.id == 'slash-command')
      this.$.toolbar.showSearchField();
    if (e.command.id == 'delete-command')
      this.deleteSelected();
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
    if (syncedDeviceManagerElem)
      syncedDeviceManagerElem.updateSignInState(isUserSignedIn);
  },

  /**
   * @param {string} selectedPage
   * @return {boolean}
   * @private
   */
  syncedTabsSelected_: function(selectedPage) {
    return selectedPage == 'syncedTabs';
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

  /**
   * @param {string} page
   * @private
   */
  routeDataChanged_: function(page) {
    this.selectedPage_ = page;
  },

  /**
   * @param {string} selectedPage
   * @private
   */
  selectedPageChanged_: function(selectedPage) {
    this.set('routeData_.page', selectedPage);
  },

  /**
   * This computed binding is needed to make the iron-pages selector update when
   * the synced-device-manager is instantiated for the first time. Otherwise the
   * fallback selection will continue to be used after the corresponding item is
   * added as a child of iron-pages.
   * @param {string} selectedPage
   * @param {Array} items
   * @return {string}
   * @private
   */
  getSelectedPage_(selectedPage, items) {
    return selectedPage;
  },

  /** @private */
  closeDrawer_: function() {
    var drawer = this.$$('#drawer');
    if (drawer)
      drawer.close();
  },
});
