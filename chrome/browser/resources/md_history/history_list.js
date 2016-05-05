// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-list',

  properties: {
    // An array of history entries in reverse chronological order.
    historyData: {
      type: Array
    },

    // The time of access of the last history item in historyData.
    lastVisitedTime: {
      type: Number,
      value: 0
    },

    searchTerm: {
      type: String,
      value: ''
    },

    // True if there is a pending request to the backend.
    loading_: {
      type: Boolean,
      value: true
    },

    resultLoadingDisabled_: {
      type: Boolean,
      value: false
    },
  },

  listeners: {
    'infinite-list.scroll': 'closeMenu_',
    'tap': 'closeMenu_',
    'toggle-menu': 'toggleMenu_',
  },

  /**
   * Closes the overflow menu.
   * @private
   */
  closeMenu_: function() {
    /** @type {CrSharedMenuElement} */(this.$.sharedMenu).closeMenu();
  },

  /**
   * Mark the page as currently loading new data from the back-end.
   */
  setLoading: function() {
    this.loading_ = true;
  },

  /**
   * Opens the overflow menu unless the menu is already open and the same button
   * is pressed.
   * @param {{detail: {itemIdentifier: !Object}}} e
   * @private
   */
  toggleMenu_: function(e) {
    var target = e.detail.target;
    /** @type {CrSharedMenuElement} */(this.$.sharedMenu).toggleMenu(
        target, e.detail.itemIdentifier);
  },

  /** @private */
  onMoreFromSiteTap_: function() {
    var menu = /** @type {CrSharedMenuElement} */(this.$.sharedMenu);
    this.fire('search-changed', {search: menu.itemData.domain});
    menu.closeMenu();
  },

  /**
   * Disables history result loading when there are no more history results.
   */
  disableResultLoading: function() {
    this.resultLoadingDisabled_ = true;
  },

  /**
   * Adds the newly updated history results into historyData. Adds new fields
   * for each result.
   * @param {!Array<!HistoryEntry>} historyResults The new history results.
   * @param {string} searchTerm Search query used to find these results.
   */
  addNewResults: function(historyResults, searchTerm) {
    this.loading_ = false;
    /** @type {IronScrollThresholdElement} */(this.$['scroll-threshold'])
        .clearTriggers();

    if (this.searchTerm != searchTerm) {
      this.resultLoadingDisabled_ = false;
      if (this.historyData)
        this.splice('historyData', 0, this.historyData.length);
      this.searchTerm = searchTerm;
    }

    if (historyResults.length == 0)
      return;

    // Creates a copy of historyResults to prevent accidentally modifying this
    // field.
    var results = historyResults.slice();

    var currentDate = results[0].dateRelativeDay;

    for (var i = 0; i < results.length; i++) {
      // Sets the default values for these fields to prevent undefined types.
      results[i].selected = false;
      results[i].readableTimestamp =
          searchTerm == '' ? results[i].dateTimeOfDay : results[i].dateShort;

      if (results[i].dateRelativeDay != currentDate) {
        currentDate = results[i].dateRelativeDay;
      }
    }

    if (this.historyData) {
      // If we have previously received data, push the new items onto the
      // existing array.
      results.unshift('historyData');
      this.push.apply(this, results);
    } else {
      // The first time we receive data, use set() to ensure the iron-list is
      // initialized correctly.
      this.set('historyData', results);
    }

    this.lastVisitedTime = this.historyData[this.historyData.length - 1].time;
  },

  /**
   * Cycle through each entry in historyData and set all items to be
   * unselected.
   * @param {number} overallItemCount The number of checkboxes selected.
   */
  unselectAllItems: function(overallItemCount) {
    if (this.historyData === undefined)
      return;

    for (var i = 0; i < this.historyData.length; i++) {
      if (this.historyData[i].selected) {
        this.set('historyData.' + i + '.selected', false);
        overallItemCount--;
        if (overallItemCount == 0)
          break;
      }
    }
  },

  /**
   * Remove all selected items from the overall array so that they are also
   * removed from view. Make sure that the card length and positioning is
   * updated accordingly.
   * @param {number} overallItemCount The number of items selected.
   */
  removeDeletedHistory: function(overallItemCount) {
    var splices = [];
    for (var i = this.historyData.length - 1; i >= 0; i--) {
      if (!this.historyData[i].selected)
        continue;

      // Removes the selected item from historyData. Use unshift so |splices|
      // ends up in index order.
      splices.unshift({
        index: i,
        removed: [this.historyData[i]],
        addedCount: 0,
        object: this.historyData,
        type: 'splice'
      });
      this.historyData.splice(i, 1);

      overallItemCount--;
      if (overallItemCount == 0)
        break;
    }
    // notifySplices gives better performance than individually splicing as it
    // batches all of the updates together.
    this.notifySplices('historyData', splices);
  },

  /**
   * Based on which items are selected, collect an array of the info required
   * for chrome.send('removeHistory', ...).
   * @param {number} count The number of items that are selected.
   * @return {Array<HistoryEntry>} toBeRemoved An array of objects which contain
   * information on which history-items should be deleted.
   */
  getSelectedItems: function(count) {
    var toBeRemoved = [];
    for (var i = 0; i < this.historyData.length; i++) {
      if (this.historyData[i].selected) {
        toBeRemoved.push({
          url: this.historyData[i].url,
          timestamps: this.historyData[i].allTimestamps
        });

        count--;
        if (count == 0)
          break;
      }
    }
    return toBeRemoved;
  },

  /**
   * Called when the page is scrolled to near the bottom of the list.
   * @private
   */
  loadMoreData_: function() {
    if (this.resultLoadingDisabled_ || this.loading_)
      return;

    this.loading_ = true;
    chrome.send('queryHistory',
        [this.searchTerm, 0, 0, this.lastVisitedTime, RESULTS_PER_PAGE]);
  },

  /**
   * Check whether the time difference between the given history item and the
   * next one is large enough for a spacer to be required.
   * @param {HistoryEntry} item
   * @param {number} index The index of |item| in |historyData|.
   * @param {number} length The length of |historyData|.
   * @return {boolean} Whether or not time gap separator is required.
   * @private
   */
  needsTimeGap_: function(item, index, length) {
    if (index >= length - 1 || length == 0)
      return false;

    var currentItem = this.historyData[index];
    var nextItem = this.historyData[index + 1];

    if (this.searchTerm)
      return currentItem.dateShort != nextItem.dateShort;

    return currentItem.time - nextItem.time > BROWSING_GAP_TIME &&
        currentItem.dateRelativeDay == nextItem.dateRelativeDay;
  },

  hasResults: function(historyDataLength) {
    return historyDataLength > 0;
  },

  noResultsMessage_: function(searchTerm, isLoading) {
    if (isLoading)
      return '';
    var messageId = searchTerm !== '' ? 'noSearchResults' : 'noResults';
    return loadTimeData.getString(messageId);
  },

  /**
   * True if the given item is the beginning of a new card.
   * @param {HistoryEntry} item
   * @param {number} i Index of |item| within |historyData|.
   * @param {number} length
   * @return {boolean}
   * @private
   */
  isCardStart_: function(item, i, length) {
    if (length == 0 || i > length - 1)
      return false;
    return i == 0 ||
        this.historyData[i].dateRelativeDay !=
        this.historyData[i - 1].dateRelativeDay;
  },

  /**
   * True if the given item is the end of a card.
   * @param {HistoryEntry} item
   * @param {number} i Index of |item| within |historyData|.
   * @param {number} length
   * @return {boolean}
   * @private
   */
  isCardEnd_: function(item, i, length) {
    if (length == 0 || i > length - 1)
      return false;
    return i == length - 1 ||
        this.historyData[i].dateRelativeDay !=
        this.historyData[i + 1].dateRelativeDay;
  },
});
