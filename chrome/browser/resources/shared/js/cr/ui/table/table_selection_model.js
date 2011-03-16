// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a multiple selection model for table
 */
cr.define('cr.ui.table', function() {
  const ListSelectionModel = cr.ui.ListSelectionModel;

  /**
   * Creates a new selection model that is to be used with tables.
   * This implementation supports multiple selection.
   * Selected items are stored, not indexes, so selections are preserved
   * after items reordering (e.g. because of sort).
   * @param {number=} opt_length The number of items in the selection.
   * @constructor
   * @extends {!cr.EventTarget}
   */
  function TableSelectionModel(opt_length) {
    ListSelectionModel.apply(this, arguments);
  }

  TableSelectionModel.prototype = {
    __proto__: ListSelectionModel.prototype,


    /**
     * Adjusts the selection after reordering of items in the table.
     * @param {!Array.<number>} permutation The reordering permutation.
     */
    adjustToReordering: function(permutation) {
      var oldSelectedIndexes = this.selectedIndexes;
      this.selectedIndexes = oldSelectedIndexes.map(function(oldIndex) {
        return permutation[oldIndex];
      }).filter(function(index) {
        return index != -1;
      });
    },

    /**
     * Adjust the selection by adding or removing a certain numbers of items.
     * This should be called by the owner of the selection model as items are
     * added and removed from the underlying data model.
     * This implementation updates selection model length only. The actual
     * selected indexes changes are processed in adjustToReordering.
     * @param {number} index The index of the first change.
     * @param {number} itemsRemoved Number of items removed.
     * @param {number} itemsAdded Number of items added.
     */
    adjust: function(index, itemsRemoved, itemsAdded) {
      ListSelectionModel.prototype.adjust.call(
          this, this.length, itemsRemoved, itemsAdded);
    }
  };

  return {
    TableSelectionModel: TableSelectionModel
  };
});
