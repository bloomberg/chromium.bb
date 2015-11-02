// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Strips a scheme from an origin.
 * @param {string} origin
 * @return {string} The host of the given origin.
 */
function stripScheme(origin) {
  return new URL(origin).host;
}

Polymer({
  is: 'engagement-table',
  properties: {
    /**
     * A list of engagement info objects.
     * @type !Array<!SiteEngagementInfo>
     */
    engagementInfo: {type: Array, value: function() { return []; } },

    /**
     * The table's current sort key.
     * @type {string}
     * @private
     */
    sortKey_: {type: String, value: 'score'},

    /**
     * Whether the table is in reverse sorting order.
     * @type {boolean}
     * @private
     */
    sortReverse: {type: Boolean, value: true, reflectToAttribute: true},
  },

  /**
   * @param {Event} e
   */
  handleSortColumnTap: function(e) {
    this.sortTable_(e.currentTarget);
    e.preventDefault();
  },

  /**
   * Sorts the engagement table based on the provided sort column header. Sort
   * columns have a 'sort-key' attribute that is a property name of a
   * SiteEngagementInfo.
   * @param {HTMLElement} th The column to sort by.
   * @private
   */
  sortTable_: function(th) {
    // Remove the old sort indicator column.
    var sortColumn = this.$$('.sort-column');
    if (sortColumn)
      sortColumn.className = '';

    // Updating these properties recomputes the template sort function.
    var newSortKey = th.getAttribute('sort-key');
    if (this.sortKey_)
      this.sortReverse = (newSortKey == this.sortKey_) && !this.sortReverse;

    this.sortKey_ = newSortKey;

    // Specify the new sort indicator column.
    th.className = 'sort-column';
  },

  /**
   * Returns a sorting function for SiteEngagementInfo objects that sorts based
   * on sortKey.
   * @param {string} sortKey The name of the property to sort by.
   * @param {boolean} reverse Whether the sort should be in reverse order.
   * @return {function} A comparator for SiteEngagementInfos.
   * @private
   */
  getTableSortFunction_: function(sortKey, reverse) {
    return function(a, b) {
      return (reverse ? -1 : 1) *
             this.compareTableItem_(sortKey, a, b);
    }.bind(this);
  },

  /**
   * Compares two SiteEngagementInfo objects based on the sortKey.
   * @param {string} sortKey The name of the property to sort by.
   * @return {number} A negative number if |a| should be ordered before |b|, a
   * positive number otherwise.
   * @private
   */
  compareTableItem_: function(sortKey, a, b) {
    var val1 = a[sortKey];
    var val2 = b[sortKey];

    if (sortKey == 'origin')
      return stripScheme(val1) > stripScheme(val2) ? 1 : -1;

    if (sortKey == 'score')
      return val1 - val2;

    assertNotReached('Unsupported sort key: ' + sortKey);
    return 0;
  },
});
