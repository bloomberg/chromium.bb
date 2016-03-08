// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-list',

  properties: {
    // An array of history entries in reverse chronological order.
    historyData: {
      type: Array,
      value: function() { return []; }
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
   * @param {Event} e The event with details of the menu item that was clicked.
   * @private
   */
  toggleMenu_: function(e) {
    var target = e.detail.target;
    /** @type {CrSharedMenuElement} */(this.$.sharedMenu).toggleMenu(target);
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
   */
  addNewResults: function(historyResults, searchTerm) {
    this.loading_ = false;

    if (this.searchTerm != searchTerm) {
      this.splice('historyData', 0, this.historyData.length);
      this.searchTerm = searchTerm;
    }

    if (historyResults.length == 0)
      return;

    // Creates a copy of historyResults to prevent accidentally modifying this
    // field.
    var results = historyResults.slice();

    var currentDate = results[0].dateRelativeDay;

    // Resets the last history item for the currentDate if new history results
    // for currentDate is loaded.
    var lastHistoryItem = this.historyData[this.historyData.length - 1];
    if (lastHistoryItem && lastHistoryItem.isLastItem &&
        lastHistoryItem.dateRelativeDay == currentDate) {
      this.set('historyData.' + (this.historyData.length - 1) +
          '.isLastItem', false);
    }

    for (var i = 0; i < results.length; i++) {
      // Sets the default values for these fields to prevent undefined types.
      results[i].selected = false;
      results[i].isLastItem = false;
      results[i].isFirstItem = false;
      results[i].needsTimeGap = this.needsTimeGap_(results, i);
      results[i].readableTimestamp =
          searchTerm == '' ? results[i].dateTimeOfDay : results[i].dateShort;

      if (results[i].dateRelativeDay != currentDate) {
        results[i - 1].isLastItem = true;
        results[i].isFirstItem = true;
        currentDate = results[i].dateRelativeDay;
      }
    }
    results[i - 1].isLastItem = true;

    // If it's the first time we get data, the first item will always be the
    // first card.
    if (this.historyData.length == 0)
      results[0].isFirstItem = true;

    // Adds results to the beginning of the historyData array.
    results.unshift('historyData');
    this.push.apply(this, results);

    this.lastVisitedTime = this.historyData[this.historyData.length - 1].time;
  },

  /**
   * Cycle through each entry in historyData and set all items to be
   * unselected.
   * @param {number} overallItemCount The number of checkboxes selected.
   */
  unselectAllItems: function(overallItemCount) {
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
    for (var i = this.historyData.length - 1; i >= 0; i--) {
      if (!this.historyData[i].selected)
        continue;

      // TODO: Change to using computed properties to recompute the first and
      // last cards.

      // Resets the first history item.
      if (this.historyData[i].isFirstItem &&
          (i + 1) < this.historyData.length &&
          this.historyData[i].dateRelativeDay ==
          this.historyData[i + 1].dateRelativeDay) {
        this.set('historyData.' + (i + 1) + '.isFirstItem', true);
      }

      // Resets the last history item.
      if (this.historyData[i].isLastItem && i > 0 &&
          this.historyData[i].dateRelativeDay ==
          this.historyData[i - 1].dateRelativeDay) {
        this.set('historyData.' + (i - 1) + '.isLastItem', true);

        if (this.historyData[i - 1].needsTimeGap)
          this.set('historyData.' + (i - 1) + '.needsTimeGap', false);
      }

      // Makes sure that the time gap separators are preserved.
      if (this.historyData[i].needsTimeGap && i > 0)
        this.set('historyData.' + (i - 1) + '.needsTimeGap', true);

      // Removes the selected item from historyData.
      this.splice('historyData', i, 1);

      overallItemCount--;
      if (overallItemCount == 0)
        break;
    }
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
   * Called when the card manager is scrolled.
   * @private
   */
  scrollHandler_: function() {
    if (this.resultLoadingDisabled_)
      return;

    // Requests the next list of results when the scrollbar is near the bottom
    // of the window.
    var scrollOffset = 10;
    var scrollElem = this.$['infinite-list'];

    if (!this.loading_ && scrollElem.scrollHeight <=
        scrollElem.scrollTop + scrollElem.clientHeight + scrollOffset) {
      this.loading_ = true;
      chrome.send('queryHistory',
          [this.searchTerm, 0, 0, this.lastVisitedTime, RESULTS_PER_PAGE]);
    }
  },

  /**
   * Check whether the time difference between the given history item and the
   * next one is large enough for a spacer to be required.
   * @param {Array<HistoryEntry>} results A list of history results.
   * @param {number} index The index number of the first item being compared.
   * @return {boolean} Whether or not time gap separator is required.
   * @private
   */
  needsTimeGap_: function(results, index) {
    // TODO(tsergeant): Allow the final item from one batch of results to have a
    // timegap once more results are added.
    if (index == results.length - 1)
      return false;

    var currentItem = results[index];
    var nextItem = results[index + 1];

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
  }
});
