// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suiteSetup(function() {
  cr.define('bookmarks', function() {
    var TestStore = function(data) {
      bookmarks.Store.call(this);
      this.data_ = Object.assign(bookmarks.util.createEmptyState(), data);
      this.initialized_ = true;

      this.lastAction_ = null;
      this.acceptInit_ = false;
      this.enableReducers_ = false;
    };

    TestStore.prototype = {
      __proto__: bookmarks.Store.prototype,

      init: function(state) {
        if (this.acceptInit_)
          bookmarks.Store.prototype.init.call(this, state);
      },

      get lastAction() {
        return this.lastAction_;
      },

      get data() {
        return this.data_;
      },

      set data(newData) {
        this.data_ = newData;
      },

      /**
       * Enable or disable calling bookmarks.reduceAction for each action.
       * With reducers disabled (the default), TestStore is a stub which
       * requires state be managed manually (suitable for unit tests). With
       * reducers enabled, TestStore becomes a proxy for observing actions
       * (suitable for integration tests).
       * @param {boolean} enabled
       */
      setReducersEnabled: function(enabled) {
        this.enableReducers_ = enabled;
      },

      reduce_: function(action) {
        this.lastAction_ = action;
        if (this.enableReducers_)
          bookmarks.Store.prototype.reduce_.call(this, action);
      },

      notifyObservers: function() {
        // TODO(tsergeant): Revisit how state modifications work in UI tests.
        // We don't want tests to worry about modifying the whole state tree.
        // Instead, we could perform a deep clone in here to ensure that every
        // StoreClient is updated.
        this.notifyObservers_(this.data);
      },

      // Call in order to accept data from an init call to the TestStore once.
      acceptInitOnce: function() {
        this.acceptInit_ = true;
        this.initialized_ = false;
      },
    };

    return {
      TestStore: TestStore,
    };
  });
});
