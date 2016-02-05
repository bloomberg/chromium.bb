// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-card',

  properties: {
    // The date of these history items.
    historyDate: {
      type: String,
      value: ''
    },

    // The list of history results that were accessed for a particular day in
    // reverse chronological order.
    historyItems: {
      type: Array,
      value: function() { return []; }
    }
  },

  /**
   * Check whether the time difference between the given historyItem and the
   * next one is large enough for a spacer to be required.
   * @param {number} index The index number of the first item being compared.
   * @param {number} itemsLength The number of items on the card. Used to force
   * needsTimeGap_ to run for every item if an item is deleted from the card.
   * @return {boolean} Whether or not time gap separator is required.
   * @private
   */
  needsTimeGap_: function(index, itemsLength) {
    var items = this.historyItems;
    return index + 1 < items.length &&
        items[index].time - items[index + 1].time > BROWSING_GAP_TIME;
  }
});
