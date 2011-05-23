// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a table data model
 */
cr.define('cr.ui.table', function() {
  const EventTarget = cr.EventTarget;
  const Event = cr.Event;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * A table data model that supports sorting by storing initial indexes of
   * elements for each position in sorted array.
   * @param {!Array} items The underlying array.
   * @constructor
   * @extends {ArrayDataModel}
   */
  function TableDataModel(items) {
    ArrayDataModel.apply(this, arguments);
    this.indexes_ = [];
    for (var i = 0; i < items.length; i++) {
      this.indexes_.push(i);
    }
  }

  TableDataModel.prototype = {
    __proto__: ArrayDataModel.prototype,

    /**
     * Returns the item at the given index.
     * This implementation returns the item at the given index in the source
     * array before sort.
     * @param {number} index The index of the element to get.
     * @return {*} The element at the given index.
     */
    getItemByUnsortedIndex_: function(unsortedIndex) {
      return ArrayDataModel.prototype.item.call(this, unsortedIndex);
    },

    /**
     * Returns the item at the given index.
     * This implementation returns the item at the given index in the sorted
     * array.
     * @param {number} index The index of the element to get.
     * @return {*} The element at the given index.
     */
    item: function(index) {
      if (index >= 0 && index < this.length)
        return this.getItemByUnsortedIndex_(this.indexes_[index]);
      return undefined;
    },

    /**
     * Returns compare function set for given field.
     * @param {string} field The field to get compare function for.
     * @return {Function(*, *): number} Compare function set for given field.
     */
    compareFunction: function(field) {
      return this.compareFunctions_[field];
    },

    /**
     * Sets compare function for given field.
     * @param {string} field The field to set compare function.
     * @param {Function(*, *): number} Compare function to set for given field.
     */
    setCompareFunction: function(field, compareFunction) {
      this.compareFunctions_[field] = compareFunction;
    },

    /**
     * Returns current sort status.
     * @return {!Object} Current sort status.
     */
    get sortStatus() {
      if (this.sortStatus_) {
        return this.createSortStatus(
            this.sortStatus_.field, this.sortStatus_.direction);
      } else {
        return this.createSortStatus(null, null);
      }
    },

    /**
     * This removes and adds items to the model.
     * This dispatches a splice event.
     * This implementation runs sort after splice and creates permutation for
     * the whole change.
     * @param {number} index The index of the item to update.
     * @param {number} deleteCount The number of items to remove.
     * @param {...*} The items to add.
     * @return {!Array} An array with the removed items.
     */
    splice: function(index, deleteCount, var_args) {
      var addCount = arguments.length - 2;
      var newIndexes = [];
      var deletePermutation = [];
      var deleted = 0;
      for (var i = 0; i < this.indexes_.length; i++) {
        var oldIndex = this.indexes_[i];
        if (oldIndex < index) {
          newIndexes.push(oldIndex);
          deletePermutation.push(i - deleted);
        } else if (oldIndex >= index + deleteCount) {
          newIndexes.push(oldIndex - deleteCount + addCount);
          deletePermutation.push(i - deleted);
        } else {
          deletePermutation.push(-1);
          deleted++;
        }
      }
      for (var i = 0; i < addCount; i++) {
        newIndexes.push(index + i);
      }
      this.indexes_ = newIndexes;

      var rv = ArrayDataModel.prototype.splice.apply(this, arguments);

      var splicePermutation;
      if (this.sortStatus.field) {
        var sortPermutation = this.doSort_(this.sortStatus.field,
                                           this.sortStatus.direction);
        splicePermutation = deletePermutation.map(function(element) {
          return element != -1 ? sortPermutation[element] : -1;
        });
      } else {
        splicePermutation = deletePermutation;
      }
      this.dispatchSortEvent_(splicePermutation);

      return rv;
    },

    /**
     * Use this to update a given item in the array. This does not remove and
     * reinsert a new item.
     * This dispatches a change event.
     * This implementation runs sort after updating.
     * @param {number} index The index of the item to update.
     */
    updateIndex: function(index) {
      ArrayDataModel.prototype.updateIndex.apply(this, arguments);

      if (this.sortStatus.field)
        this.sort(this.sortStatus.field, this.sortStatus.direction);
    },

    /**
     * Creates sort status with given field and direction.
     * @param {string} field Sort field.
     * @param {string} direction Sort direction.
     * @return {!Object} Created sort status.
     */
    createSortStatus: function(field, direction) {
      return {
        field: field,
        direction: direction
      };
    },

    /**
     * Called before a sort happens so that you may fetch additional data
     * required for the sort.
     *
     * @param {string} field Sort field.
     * @param {function()} callback The function to invoke when preparation
     *     is complete.
     */
    prepareSort: function(field, callback) {
      callback();
    },

    /**
     * Sorts data model according to given field and direction and dispathes
     * sorted event.
     * @param {string} field Sort field.
     * @param {string} direction Sort direction.
     */
    sort: function(field, direction) {
      var self = this;

      this.prepareSort(field, function() {
        var sortPermutation = self.doSort_(field, direction);
        self.dispatchSortEvent_(sortPermutation);
      });
    },

    /**
     * Sorts data model according to given field and direction.
     * @param {string} field Sort field.
     * @param {string} direction Sort direction.
     */
    doSort_: function(field, direction) {
      var compareFunction = this.sortFunction_(field, direction);
      var positions = [];
      for (var i = 0; i < this.length; i++) {
        positions[this.indexes_[i]] = i;
      }
      this.indexes_.sort(compareFunction);
      this.sortStatus_ = this.createSortStatus(field, direction);
      var sortPermutation = [];
      for (var i = 0; i < this.length; i++) {
        sortPermutation[positions[this.indexes_[i]]] = i;
      }
      return sortPermutation;
    },

    dispatchSortEvent_: function(sortPermutation) {
      var e = new Event('sorted');
      e.sortPermutation = sortPermutation;
      this.dispatchEvent(e);
    },

    /**
     * Creates compare function for the field.
     * Returns the function set as sortFunction for given field
     * or default compare function
     * @param {string} field Sort field.
     * @param {Function(*, *): number} Compare function.
     */
    createCompareFunction_: function(field) {
      var compareFunction =
          this.compareFunctions_ ? this.compareFunctions_[field] : null;
      var defaultValuesCompareFunction = this.defaultValuesCompareFunction;
      if (compareFunction) {
        return compareFunction;
      } else {
        return function(a, b) {
          return defaultValuesCompareFunction.call(null, a[field], b[field]);
        }
      }
      return compareFunction;
    },

    /**
     * Creates compare function for given field and direction.
     * @param {string} field Sort field.
     * @param {string} direction Sort direction.
     * @param {Function(*, *): number} Compare function.
     */
    sortFunction_: function(field, direction) {
      var compareFunction = this.createCompareFunction_(field);
      var dirMultiplier = direction == 'desc' ? -1 : 1;

      return function(index1, index2) {
        var item1 = this.getItemByUnsortedIndex_(index1);
        var item2 = this.getItemByUnsortedIndex_(index2);

        var compareResult = compareFunction.call(null, item1, item2);
        if (compareResult != 0)
          return dirMultiplier * compareResult;
        return dirMultiplier * this.defaultValuesCompareFunction(index1,
                                                                 index2);
      }.bind(this);
    },

    /**
     * Default compare function.
     */
    defaultValuesCompareFunction: function(a, b) {
      // We could insert i18n comparisons here.
      if (a < b)
        return -1;
      if (a > b)
        return 1;
      return 0;
    }
  };

  return {
    TableDataModel: TableDataModel
  };
});
