// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-list-container',

  properties: {
    // The path of the currently selected page.
    selectedPage_: {
      type: String,
      computed: 'computeSelectedPage_(queryState.range)',
    },

    // Whether domain-grouped history is enabled.
    grouped: Boolean,

    /** @type {!QueryState} */
    queryState: Object,

    /** @type {!QueryResult} */
    queryResult: Object,

    /**
     * @private {?{
     *   index: number,
     *   item: !HistoryEntry,
     *   path: string,
     *   target: !HTMLElement
     * }}
     */
    actionMenuModel_: Object,
  },

  observers: [
    'groupedRangeChanged_(queryState.range)',
  ],

  listeners: {
    'open-menu': 'openMenu_',
  },

  /**
   * @param {HistoryQuery} info An object containing information about the
   *    query.
   * @param {!Array<!HistoryEntry>} results A list of results.
   */
  historyResult: function(info, results) {
    this.initializeResults_(info, results);
    this.closeMenu_();

    if (info.term && !this.queryState.incremental) {
      Polymer.IronA11yAnnouncer.requestAvailability();
      this.fire('iron-announce', {
        text:
            md_history.HistoryItem.searchResultsTitle(results.length, info.term)
      });
    }

    var list = /** @type {HistoryListBehavior} */ this.getSelectedList_();
    list.addNewResults(results, this.queryState.incremental, info.finished);
  },

  historyDeleted: function() {
    // Do not reload the list when there are items checked.
    if (this.getSelectedItemCount() > 0)
      return;

    // Reload the list with current search state.
    this.fire('query-history', false);
  },

  /** @return {Element} */
  getContentScrollTarget: function() {
    return this.getSelectedList_();
  },

  /** @return {number} */
  getSelectedItemCount: function() {
    return this.getSelectedList_().selectedPaths.size;
  },

  unselectAllItems: function(count) {
    var selectedList = this.getSelectedList_();
    if (selectedList)
      selectedList.unselectAllItems(count);
  },

  /**
   * Delete all the currently selected history items. Will prompt the user with
   * a dialog to confirm that the deletion should be performed.
   */
  deleteSelectedWithPrompt: function() {
    if (!loadTimeData.getBoolean('allowDeletingHistory'))
      return;

    var browserService = md_history.BrowserService.getInstance();
    browserService.recordAction('RemoveSelected');
    if (this.queryState.searchTerm != '')
      browserService.recordAction('SearchResultRemove');
    this.$.dialog.get().showModal();

    // TODO(dbeam): remove focus flicker caused by showModal() + focus().
    this.$$('.action-button').focus();
  },

  /**
   * @param {HistoryRange} range
   * @return {string}
   * @private
   */
  computeSelectedPage_: function(range) {
    return range == HistoryRange.ALL_TIME ? 'infinite-list' : 'grouped-list';
  },

  /**
   * @param {HistoryRange} range
   * @private
   */
  groupedRangeChanged_: function(range) {
    // Reset the results on range change to prevent stale results from being
    // processed into the incoming range's UI.
    if (range != HistoryRange.ALL_TIME && this.queryResult.info) {
      this.set('queryResult.results', []);
      this.historyResult(this.queryResult.info, []);
    }
  },

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
    md_history.BrowserService.getInstance().recordAction(
        'ConfirmRemoveSelected');

    this.getSelectedList_().deleteSelected();
    var dialog = assert(this.$.dialog.getIfExists());
    dialog.close();
  },

  /** @private */
  onDialogCancelTap_: function() {
    md_history.BrowserService.getInstance().recordAction(
        'CancelRemoveSelected');

    var dialog = assert(this.$.dialog.getIfExists());
    dialog.close();
  },

  /**
   * Closes the overflow menu.
   * @private
   */
  closeMenu_: function() {
    var menu = this.$.sharedMenu.getIfExists();
    if (menu && menu.open) {
      this.actionMenuModel_ = null;
      menu.close();
    }
  },

  /**
   * Opens the overflow menu.
   * @param {{detail: {
   *    index: number, item: !HistoryEntry,
   *    path: string, target: !HTMLElement
   * }}} e
   * @private
   */
  openMenu_: function(e) {
    var target = e.detail.target;
    this.actionMenuModel_ = e.detail;
    var menu = /** @type {CrSharedMenuElement} */ this.$.sharedMenu.get();
    menu.showAt(target);
  },

  /** @private */
  onMoreFromSiteTap_: function() {
    md_history.BrowserService.getInstance().recordAction(
        'EntryMenuShowMoreFromSite');

    var menu = assert(this.$.sharedMenu.getIfExists());
    this.fire('change-query', {search: this.actionMenuModel_.item.domain});
    this.actionMenuModel_ = null;
    this.closeMenu_();
  },

  /** @private */
  onRemoveFromHistoryTap_: function() {
    var browserService = md_history.BrowserService.getInstance();
    browserService.recordAction('EntryMenuRemoveFromHistory');
    var menu = assert(this.$.sharedMenu.getIfExists());
    var itemData = this.actionMenuModel_;
    browserService.deleteItems([itemData.item]).then(function(items) {
      // This unselect-all resets the toolbar when deleting a selected item
      // and clears selection state which can be invalid if items move
      // around during deletion.
      // TODO(tsergeant): Make this automatic based on observing list
      // modifications.
      this.fire('unselect-all');
      this.getSelectedList_().removeItemsByPath([itemData.path]);

      var index = itemData.index;
      if (index == undefined)
        return;

      var browserService = md_history.BrowserService.getInstance();
      browserService.recordHistogram(
          'HistoryPage.RemoveEntryPosition',
          Math.min(index, UMA_MAX_BUCKET_VALUE), UMA_MAX_BUCKET_VALUE);
      if (index <= UMA_MAX_SUBSET_BUCKET_VALUE) {
        browserService.recordHistogram(
            'HistoryPage.RemoveEntryPositionSubset', index,
            UMA_MAX_SUBSET_BUCKET_VALUE);
      }
    }.bind(this));
    this.closeMenu_();
  },

  /**
   * @return {Element}
   * @private
   */
  getSelectedList_: function() {
    return this.$$('#' + this.selectedPage_);
  },

  /** @private */
  canDeleteHistory_: function() {
    return loadTimeData.getBoolean('allowDeletingHistory');
  }
});
