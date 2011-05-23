// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a data model representin
 */

cr.define('cr.ui', function() {
  const EventTarget = cr.EventTarget;
  const Event = cr.Event;

  /**
   * A data model that wraps a simple array.
   * @param {!Array} array The underlying array.
   * @constructor
   * @extends {EventTarget}
   */
  function ArrayDataModel(array) {
    this.array_ = array;
  }

  ArrayDataModel.prototype = {
    __proto__: EventTarget.prototype,

    /**
     * The length of the data model.
     * @type {number}
     */
    get length() {
      return this.array_.length;
    },

    /**
     * Returns the item at the given index.
     * @param {number} index The index of the element to get.
     * @return {*} The element at the given index.
     */
    item: function(index) {
      return this.array_[index];
    },

    /**
     * Returns the first matching item.
     * @param {*} item The item to find.
     * @param {number=} opt_fromIndex If provided, then the searching start at
     *     the {@code opt_fromIndex}.
     * @return {number} The index of the first found element or -1 if not found.
     */
    indexOf: function(item, opt_fromIndex) {
      return this.array_.indexOf(item, opt_fromIndex);
    },

    /**
     * Returns an array of elements in a selected range.
     * @param {number=} opt_from The starting index of the selected range.
     * @param {number=} opt_to The ending index of selected range.
     * @return {Array} An array of elements in the selected range.
     */
    slice: function(opt_from, opt_to) {
      return this.array_.slice.apply(this.array_, arguments);
    },

    /**
     * This removes and adds items to the model.
     *
     * This dispatches a splice event.
     *
     * @param {number} index The index of the item to update.
     * @param {number} deleteCount The number of items to remove.
     * @param {...*} The items to add.
     * @return {!Array} An array with the removed items.
     */
    splice: function(index, deleteCount, var_args) {
      var arr = this.array_;

      // TODO(arv): Maybe unify splice and change events?
      var e = new Event('splice');
      e.index = index;
      e.removed = arr.slice(index, index + deleteCount);
      e.added = Array.prototype.slice.call(arguments, 2);

      var rv = arr.splice.apply(arr, arguments);

      this.dispatchEvent(e);

      return rv;
    },

    /**
     * Appends items to the end of the model.
     *
     * This dispatches a splice event.
     *
     * @param {...*} The items to append.
     * @return {number} The new length of the model.
     */
    push: function(var_args) {
      var args = Array.prototype.slice.call(arguments);
      args.unshift(this.length, 0);
      this.splice.apply(this, args);
      return this.length;
    },

    /**
     * Use this to update a given item in the array. This does not remove and
     * reinsert a new item.
     *
     * This dispatches a change event.
     *
     * @param {number} index The index of the item to update.
     */
    updateIndex: function(index) {
      if (index < 0 || index >= this.length)
        throw Error('Invalid index, ' + index);

      // TODO(arv): Maybe unify splice and change events?
      var e = new Event('change');
      e.index = index;
      this.dispatchEvent(e);
    }
  };

  return {
    ArrayDataModel: ArrayDataModel
  };
});
