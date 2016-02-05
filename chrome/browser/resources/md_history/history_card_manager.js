// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-card-manager',

  properties: {
    // An array of objects sorted in reverse chronological order.
    // Each object has a date and the history items belonging to that date.
    historyDataByDay_: {
      type: Array,
      value: function() { return []; }
    },

    // The time of access of the last element of historyDataByDay_.
    lastVisitedTime: {
      type: Number,
      value: 0
    },

    menuOpen: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },

    menuIdentifier: {
      type: Number,
      value: 0
    }
  },

  /** @const @private */
  X_OFFSET_: 30,

  listeners: {
    'tap': 'closeMenu',
    'toggle-menu': 'toggleMenu_'
  },

  /**
   * Closes the overflow menu.
   */
  closeMenu: function() {
    this.menuOpen = false;
  },

  /**
   * Opens the overflow menu unless the menu is already open and the same button
   * is pressed.
   * @param {Event} e The event with details of the menu item that was clicked.
   * @private
   */
  toggleMenu_: function(e) {
    var menu = this.$['overflow-menu'];

    // Menu closes if the same button is clicked.
    if (this.menuOpen && this.menuIdentifier == e.detail.accessTime) {
      this.closeMenu();
    } else {
      this.menuOpen = true;
      this.menuIdentifier = e.detail.accessTime;

      cr.ui.positionPopupAtPoint(e.detail.x + this.X_OFFSET_, e.detail.y, menu,
          cr.ui.AnchorType.BEFORE);

      menu.focus();
    }
  },

  /**
   * Split the newly updated history results into history items sorted via day
   * accessed.
   * @param {!Array<!HistoryEntry>} results The new history results.
   */
  addNewResults: function(results) {
    if (results.length == 0)
      return;

    var dateSortedData = [];
    var historyItems = [];
    var currentDate = results[0].dateRelativeDay;

    for (var i = 0; i < results.length; i++) {
      if (!currentDate)
        continue;

      results[i].selected = false;
      if (results[i].dateRelativeDay != currentDate) {
        this.appendHistoryData_(currentDate, historyItems);
        currentDate = results[i].dateRelativeDay;
        historyItems = [];
      }
      historyItems.push(results[i]);
    }

    if (currentDate)
      this.appendHistoryData_(currentDate, historyItems);

    this.lastVisitedTime = historyItems[historyItems.length - 1].time;
  },

  /**
   * Cycle through each entry in historyDataByDay_ and set all items to be
   * unselected.
   * @param {number} overallItemCount The number of items selected.
   */
  unselectAllItems: function(overallItemCount) {
    var historyCardData = this.historyDataByDay_;

    for (var i = 0; i < historyCardData.length; i++) {
      var items = historyCardData[i].historyItems;
      for (var j = 0; j < items.length; j++) {
        if (items[j].selected) {
          this.set('historyDataByDay_.' + i + '.historyItems.' + j +
              '.selected', false);
          overallItemCount--;
          if (overallItemCount == 0)
            return;
        }
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
    for (var i = 0; i < this.historyDataByDay_.length; i++) {
      var items = this.historyDataByDay_[i].historyItems;
      var itemDeletedFromCard = false;

      for (var j = items.length - 1; j >= 0; j--) {
        if (!items[j].selected)
          continue;

        this.splice('historyDataByDay_.' + i + '.historyItems', j, 1);
        itemDeletedFromCard = true;
        overallItemCount--;
        if (overallItemCount == 0) {
          this.removeEmptyCards_();
          // If the last card has been removed don't try to update its size.
          if (i < this.historyDataByDay_.length)
            this.$['infinite-list'].updateSizeForItem(i);
          return;
        }
      }
      if (itemDeletedFromCard)
        this.$['infinite-list'].updateSizeForItem(i);
    }
  },

  /**
   * If a day has had all the history it contains removed, remove this day from
   * the array.
   * @private
   */
  removeEmptyCards_: function() {
    var historyCards = this.historyDataByDay_;
    for (var i = historyCards.length - 1; i >= 0; i--) {
      if (historyCards[i].historyItems.length == 0) {
        this.splice('historyDataByDay_', i, 1);
      }
    }
  },

  /**
   * Adds the given items into historyDataByDay_. Adds items to the last
   * existing day if the date matches, creates a new element otherwise.
   * @param {string} date The date of the history items.
   * @param {!Array<!HistoryEntry>} historyItems The list of history items for
   * the current date.
   * @private
   */
  appendHistoryData_: function(date, historyItems) {
    var lastDay = this.historyDataByDay_.length - 1;
    if (lastDay > 0 && date == this.historyDataByDay_[lastDay].date) {
      this.set('historyDataByDay_.' + lastDay + '.historyItems',
          this.historyDataByDay_[lastDay].historyItems.concat(historyItems));
    } else {
      this.push('historyDataByDay_', {
        date: date,
        historyItems: historyItems
      });
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
    for (var i = 0; i < this.historyDataByDay_.length; i++) {
      var items = this.historyDataByDay_[i].historyItems;
      for (var j = 0; j < items.length; j++) {
        if (items[j].selected) {

          toBeRemoved.push({
            url: items[j].url,
            timestamps: items[j].allTimestamps
          });

          count--;
          if (count == 0)
            return toBeRemoved;
        }
      }
    }
    return toBeRemoved;
  },

  /**
   * Called when the card manager is scrolled.
   * @private
   */
  scrollHandler_: function() {
    // Close overflow menu on scroll.
    this.closeMenu();

    // Requests the next list of results when the scrollbar is near the bottom
    // of the window.
    var scrollOffset = 10;
    var scrollElem = this.$['infinite-list'];

    if (scrollElem.scrollHeight <=
        scrollElem.scrollTop + scrollElem.clientHeight + scrollOffset) {
      chrome.send('queryHistory',
          ['', 0, 0, this.lastVisitedTime, RESULTS_PER_PAGE]);
    }
  }
});
