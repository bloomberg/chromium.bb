// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('media', function() {

  /**
   * This class stores hashes by their id field and provides basic methods for
   * iterating over the collection.
   * @constructor
   */
  function ItemStore() {
    this.items_ = {};
  }

  ItemStore.prototype = {
    /**
     * Get a sorted list of item ids.
     * @return {Array} A sorted array of ids.
     */
    ids: function() {
      var ids = [];
      for (var i in this.items_)
        ids.push(i);
      return ids.sort();
    },

    /**
     * Add an item to the store.
     * @param {Object} item The item to be added.
     * @param {string} item.id The id of the item.
     */
    addItem: function(item) {
      this.items_[item.id] = item;
    },

    /**
     * Add a dictionary of items to the store.
     * @param {Object} items A dictionary of individual items. The keys are
     *    irrelevant but each must have an id field.
     */
    addItems: function(items) {
      for (id in items)
        this.addItem(items[id]);
    },

    /**
     * Remove an item from the store.
     * @param {string} id The id of the item to be removed.
     */
    removeItem: function(id) {
      delete this.items_[id];
    },

    /**
     * Map this itemStore to an Array. Items are sorted by id.
     * @param {function(*)} mapper The mapping function applied to each item.
     * @return {Array} An array of mapped items.
     */
    map: function(mapper) {
      var items = this.items_;
      var ids = this.ids();
      return ids.map(function(id) { return mapper(items[id]); });
    }
  };

  return {
    ItemStore: ItemStore
  };
});
