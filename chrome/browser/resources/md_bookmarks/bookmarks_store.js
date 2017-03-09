// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A singleton datastore for the Bookmarks page. Page state is
 * publicly readable, but can only be modified by dispatching an Action to
 * the store.
 */

cr.define('bookmarks', function() {
  /** @constructor */
  function Store() {
    /** @type {!BookmarksPageState} */
    this.data_ = bookmarks.util.createEmptyState();
    /** @type {boolean} */
    this.initialized_ = false;
    /** @type {!Array<!StoreObserver>} */
    this.observers_ = [];
  }

  Store.prototype = {
    /**
     * @param {!BookmarksPageState} initialState
     */
    init: function(initialState) {
      this.data_ = initialState;
      this.initialized_ = true;
      this.notifyObservers_(this.data_);
    },

    /** @type {!BookmarksPageState} */
    get data() {
      return this.data_;
    },

    /** @return {boolean} */
    isInitialized: function() {
      return this.initialized_;
    },

    /** @param {!StoreObserver} observer */
    addObserver: function(observer) {
      this.observers_.push(observer);
    },

    /** @param {!StoreObserver} observer */
    removeObserver: function(observer) {
      var index = this.observers_.indexOf(observer);
      this.observers_.splice(index, 1);
    },

    /**
     * Transition to a new UI state based on the supplied |action|, and notify
     * observers of the change.
     * @param {Action} action
     */
    handleAction: function(action) {
      if (!this.initialized_)
        return;

      this.data_ = bookmarks.reduceAction(this.data_, action);
      this.notifyObservers_(this.data_);
    },

    /**
     * @param {!BookmarksPageState} state
     * @private
     */
    notifyObservers_: function(state) {
      this.observers_.forEach(function(o) {
        o.onStateChanged(state);
      });
    },
  };

  cr.addSingletonGetter(Store);

  return {
    Store: Store,
  };
});
