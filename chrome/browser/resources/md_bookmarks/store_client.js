// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines StoreClient, a Polymer behavior to tie a front-end
 * element to back-end data from the store.
 */

cr.define('bookmarks', function() {
  /**
   * @polymerBehavior
   */
  const BookmarksStoreClientImpl = {
    /**
     * @param {string} localProperty
     * @param {function(!BookmarksPageState)} valueGetter
     */
    watch: function(localProperty, valueGetter) {
      this.watch_(localProperty, valueGetter);
    },

    /**
     * @return {BookmarksPageState}
     */
    getState: function() {
      return this.getStore().data;
    },

    /**
     * @return {bookmarks.Store}
     */
    getStore: function() {
      return bookmarks.Store.getInstance();
    },
  };

  /**
   * @polymerBehavior
   * @implements {cr.ui.StoreObserver<BookmarksPageState>}
   */
  const StoreClient = [cr.ui.StoreClient, BookmarksStoreClientImpl];

  return {
    BookmarksStoreClientImpl: BookmarksStoreClientImpl,
    StoreClient: StoreClient,
  };
});
