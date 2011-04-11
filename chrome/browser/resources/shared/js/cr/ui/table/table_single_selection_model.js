// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a single selection model for table
 */
cr.define('cr.ui.table', function() {
  const ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Creates a new selection model that is to be used with tables.
   * This implementation supports single selection.
   * Selected item is stored, not index, so selection is preserved
   * after items reordering (e.g. because of sort).
   * @param {number=} opt_length The number of items in the selection.
   * @constructor
   * @extends {!cr.EventTarget}
   */
  function TableSingleSelectionModel(opt_length) {
    ListSingleSelectionModel.apply(this, arguments);
  }

  TableSingleSelectionModel.prototype = {
    __proto__: ListSingleSelectionModel.prototype,


    /**
     * Adjusts the selection after reordering of items in the table.
     * @param {!Array.<number>} permutation The reordering permutation.
     */
    adjustToReordering: function(permutation) {
      if (this.leadIndex != -1)
        this.leadIndex = permutation[this.leadIndex];

      var oldSelectedIndex = this.selectedIndex;
      if (oldSelectedIndex != -1) {
        this.selectedIndex = permutation[oldSelectedIndex];
      }
    },

    /**
     * Adjust the selection by adding or removing a certain numbers of items.
     * This should be called by the owner of the selection model as items are
     * added and removed from the underlying data model.
     * @param {number} index The index of the first change.
     * @param {number} itemsRemoved Number of items removed.
     * @param {number} itemsAdded Number of items added.
     */
    adjust: function(index, itemsRemoved, itemsAdded) {
      ListSingleSelectionModel.prototype.adjust.call(
          this, this.length, itemsRemoved, itemsAdded);
    }
  };

  return {
    TableSingleSelectionModel: TableSingleSelectionModel
  };
});
